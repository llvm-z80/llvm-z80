//===- z80-run.cpp - Run a Z80 program and check the result ---------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// z80-run is a Z80 code runner that uses the z88dk-ticks CPU core. The goal is
// to read a linked ELF or Intel HEX file and test the program quickly.
//
//===----------------------------------------------------------------------===//

#include "Loader.h"
#include "TicksGlue.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <cstring>

using namespace llvm;
using namespace llvm::z80run;

namespace {

enum ExitCode {
  ExitPass = 0,
  ExitFail = 1,
  ExitCannotRun = 2,
};

enum class CPUOption { FromInput, Z80, SM83 };

cl::OptionCategory Category("z80-run options");

cl::opt<std::string> InputPath(cl::Positional, cl::Required,
                               cl::desc("<ELF or Intel HEX file>"),
                               cl::cat(Category));

cl::opt<CPUOption>
    CPUFlag("cpu",
            cl::desc("CPU to emulate. Taken from the ELF flags by default"),
            cl::values(clEnumValN(CPUOption::Z80, "z80", "Z80"),
                       clEnumValN(CPUOption::SM83, "sm83", "SM83")),
            cl::init(CPUOption::FromInput), cl::cat(Category));

cl::opt<std::string>
    ResultFlag("result",
               cl::desc("Where the result is read when the program halts: de, "
                        "bc, hl, or the name of a 16-bit variable. Defaults to "
                        "de on the Z80 and bc on the SM83"),
               cl::value_desc("location"), cl::cat(Category));

cl::opt<std::string>
    ExpectFlag("expect-result",
               cl::desc("Value the result must have, or 'any' to accept "
                        "whatever the program returns"),
               cl::value_desc("value"), cl::init("0"), cl::cat(Category));

cl::opt<std::string> MaxCyclesFlag(
    "max-cycles",
    cl::desc("T-states the program may spend. Accepts a k, M, G or T suffix"),
    cl::value_desc("n"), cl::init("10G"), cl::cat(Category));

cl::opt<std::string>
    ReportPath("report",
               cl::desc("Write the outcome as JSON to this file, or to stdout "
                        "for '-'"),
               cl::value_desc("file"), cl::cat(Category));

cl::opt<bool> Verbose("v", cl::desc("Describe the outcome even when it passes"),
                      cl::cat(Category));

struct ResultLocation {
  std::optional<Z80RunReg> Reg;
  uint16_t Addr = 0;
};

std::optional<uint64_t> parseCycles(StringRef Text) {
  uint64_t Scale = StringSwitch<uint64_t>(Text.take_back())
                       .Cases({"k", "K"}, 1000ULL)
                       .Case("M", 1000000ULL)
                       .Case("G", 1000000000ULL)
                       .Case("T", 1000000000000ULL)
                       .Default(1);
  if (Scale != 1)
    Text = Text.drop_back();
  uint64_t Value;
  if (Text.getAsInteger(10, Value) || Value == 0 || Value > UINT64_MAX / Scale)
    return std::nullopt;
  return Value * Scale;
}

std::optional<uint16_t> parseValue(StringRef Text) {
  int64_t Value;
  if (Text.getAsInteger(0, Value) || Value < -32768 || Value > 0xFFFF)
    return std::nullopt;
  return uint16_t(Value);
}

StringRef cpuName(Z80RunCPU CPU) {
  return CPU == Z80RunCPUSM83 ? "sm83" : "z80";
}

StringRef stopName(Z80RunStop Stop) {
  switch (Stop) {
  case Z80RunStopHalt:
    return "halt";
  case Z80RunStopBudget:
    return "budget";
  case Z80RunStopHang:
    return "hang";
  case Z80RunStopIllegal:
  case Z80RunStopTrap:
    return "fault";
  }
  llvm_unreachable("unknown stop reason");
}

std::string hex16(uint16_t Value) {
  std::string Text;
  raw_string_ostream(Text) << format_hex(Value, 6, /*Upper=*/true);
  return Text;
}

bool writeReport(json::Object Report) {
  if (ReportPath.empty())
    return true;
  std::error_code EC;
  raw_fd_ostream OS(ReportPath, EC, sys::fs::OF_None);
  if (EC) {
    WithColor::error(errs(), "z80-run")
        << "cannot write '" << ReportPath << "': " << EC.message() << '\n';
    return false;
  }
  OS << json::Value(std::move(Report)) << '\n';
  return true;
}

int cannotRun(StringRef Kind, const Twine &Message) {
  WithColor::error(errs(), "z80-run") << Message << '\n';
  writeReport(json::Object{{"error", Kind}, {"message", Message.str()}});
  return ExitCannotRun;
}

} // namespace

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::HideUnrelatedOptions(Category);
  if (!cl::ParseCommandLineOptions(
          argc, argv, "Runs a linked Z80 program and checks the result\n",
          &errs()))
    return ExitCannotRun;

  std::optional<uint64_t> MaxCycles = parseCycles(MaxCyclesFlag);
  if (!MaxCycles)
    return cannotRun("usage", "--max-cycles expects a positive count, not '" +
                                  MaxCyclesFlag + "'");
  bool ExpectAny = ExpectFlag == "any";
  std::optional<uint16_t> Expected;
  if (!ExpectAny) {
    Expected = parseValue(ExpectFlag);
    if (!Expected)
      return cannotRun(
          "usage", "--expect-result expects a 16-bit value or 'any', not '" +
                       ExpectFlag + "'");
  }

  llvm::Expected<Program> ProgramOrErr = loadProgram(InputPath);
  if (!ProgramOrErr) {
    std::string Kind = "internal";
    std::string Message;
    handleAllErrors(
        ProgramOrErr.takeError(),
        [&](const LoadError &E) {
          Kind = E.kind().str();
          Message = InputPath + ": " + E.message();
        },
        [&](const ErrorInfoBase &E) { Message = E.message(); });
    return cannotRun(Kind, Message);
  }
  Program &P = *ProgramOrErr;

  std::optional<Z80RunCPU> Requested;
  if (CPUFlag == CPUOption::Z80)
    Requested = Z80RunCPUZ80;
  else if (CPUFlag == CPUOption::SM83)
    Requested = Z80RunCPUSM83;
  if (!Requested && !P.CPU)
    return cannotRun("usage", "--cpu is required for Intel HEX input");
  if (Requested && P.CPU && *Requested != *P.CPU)
    return cannotRun("cpu-mismatch", Twine(InputPath) + " is built for " +
                                         cpuName(*P.CPU) + ", not " +
                                         cpuName(*Requested));
  Z80RunCPU CPU = Requested ? *Requested : *P.CPU;

  ResultLocation Result;
  StringRef ResultName = ResultFlag;
  if (ResultName.empty())
    ResultName = CPU == Z80RunCPUSM83 ? "bc" : "de";
  Result.Reg = StringSwitch<std::optional<Z80RunReg>>(ResultName.lower())
                   .Case("de", Z80RunRegDE)
                   .Case("bc", Z80RunRegBC)
                   .Case("hl", Z80RunRegHL)
                   .Default(std::nullopt);
  if (!Result.Reg) {
    auto It = P.Symbols.find(ResultName);
    if (It == P.Symbols.end())
      return cannotRun("usage", "--result names '" + ResultName +
                                    "', which is neither a register nor a "
                                    "symbol of " +
                                    Twine(InputPath));
    Result.Addr = It->second;
  }

  z80RunInit(CPU);
  std::memcpy(z80RunMemory(), P.Image.data(), P.Image.size());
  z80RunSetPC(P.Entry);
  Z80RunResult Run = z80RunExecute(*MaxCycles);

  json::Object Report{{"file", InputPath},
                      {"cpu", cpuName(CPU)},
                      {"stop", stopName(Run.Stop)},
                      {"pc", Run.PC},
                      {"cycles", int64_t(Run.Cycles)}};
  int Exit = ExitFail;
  std::string Message;
  switch (Run.Stop) {
  case Z80RunStopHalt: {
    uint16_t Value;
    if (Result.Reg) {
      Value = z80RunReg(*Result.Reg);
    } else {
      const uint8_t *Memory = z80RunMemory();
      Value = Memory[Result.Addr] | Memory[uint16_t(Result.Addr + 1)] << 8;
    }
    Report["result"] = Value;
    Report["expected"] =
        ExpectAny ? json::Value("any") : json::Value(*Expected);
    if (ExpectAny || Value == *Expected) {
      Exit = ExitPass;
      Message = "halted at " + hex16(Run.PC) + " after " +
                std::to_string(Run.Cycles) + " cycles with " + hex16(Value);
    } else {
      Message = "result " + hex16(Value) + ", expected " + hex16(*Expected);
    }
    break;
  }
  case Z80RunStopBudget:
    Message = "did not halt within " + std::to_string(*MaxCycles) +
              " cycles; last at " + hex16(Run.PC);
    break;
  case Z80RunStopHang:
    Message = "endless loop: the instruction at " + hex16(Run.PC) +
              " jumps to itself";
    break;
  case Z80RunStopIllegal:
    Report["fault"] = "illegal-opcode";
    Message = formatv("opcode {0} at {1} locks up the SM83",
                      format_hex(z80RunMemory()[Run.PC], 4, /*Upper=*/true),
                      hex16(Run.PC))
                  .str();
    break;
  case Z80RunStopTrap:
    Report["fault"] = "trap";
    Message = "ED FE trap at " + hex16(Run.PC);
    break;
  }

  if (Exit != ExitPass || Verbose)
    errs() << "z80-run: " << InputPath << ": " << Message << '\n';
  if (!writeReport(std::move(Report)))
    return ExitCannotRun;
  return Exit;
}
