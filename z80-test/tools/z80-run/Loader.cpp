//===- Loader.cpp - Load a program image for z80-run ----------------------===//
//
// Part of LLVM-Z80, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Loader.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cstring>

using namespace llvm;
using namespace llvm::z80run;

char LoadError::ID = 0;

static Error failure(StringRef Kind, const Twine &Message) {
  return make_error<LoadError>(Kind, Message);
}

static Expected<Program> loadELF(MemoryBufferRef Buffer) {
  Expected<std::unique_ptr<object::ObjectFile>> ObjOrErr =
      object::ObjectFile::createELFObjectFile(Buffer);
  if (!ObjOrErr)
    return failure("not-z80", toString(ObjOrErr.takeError()));
  auto *Obj = dyn_cast<object::ELF32LEObjectFile>(ObjOrErr->get());
  if (!Obj)
    return failure("not-z80", "not a 32-bit little-endian ELF file");

  const object::ELF32LEFile &File = Obj->getELFFile();
  const object::ELF32LE::Ehdr &Header = File.getHeader();
  if (Header.e_machine != ELF::EM_Z80)
    return failure("not-z80", "not a Z80 ELF file");
  if (Header.e_type != ELF::ET_EXEC)
    return failure("not-z80", "not an executable; link the program first");

  Program P;
  switch (Header.e_flags & ELF::EF_Z80_MACH_MSK) {
  case ELF::EF_Z80_MACH_Z80:
    P.CPU = Z80RunCPUZ80;
    break;
  case ELF::EF_Z80_MACH_SM83:
    P.CPU = Z80RunCPUSM83;
    break;
  default:
    return failure("unsupported-cpu",
                   "the ELF flags name a CPU z80-run does not emulate");
  }

  auto Phdrs = File.program_headers();
  if (!Phdrs)
    return failure("not-z80", toString(Phdrs.takeError()));
  StringRef Data = Buffer.getBuffer();
  for (const object::ELF32LE::Phdr &Phdr : *Phdrs) {
    if (Phdr.p_type != ELF::PT_LOAD)
      continue;
    if (uint64_t(Phdr.p_paddr) + Phdr.p_memsz > P.Image.size())
      return failure("too-large",
                     "the program does not fit in the 64 KB address space");
    if (uint64_t(Phdr.p_offset) + Phdr.p_filesz > Data.size())
      return failure("not-z80", "a segment runs past the end of the file");
    std::memcpy(&P.Image[Phdr.p_paddr], Data.data() + Phdr.p_offset,
                Phdr.p_filesz);
  }

  if (Header.e_entry >= P.Image.size())
    return failure("not-z80", "the entry point is outside the address space");
  P.Entry = Header.e_entry;

  for (const object::SymbolRef &Sym : Obj->symbols()) {
    Expected<StringRef> Name = Sym.getName();
    Expected<uint64_t> Addr = Sym.getAddress();
    if (!Name || !Addr) {
      consumeError(Name.takeError());
      consumeError(Addr.takeError());
      continue;
    }
    if (!Name->empty() && *Addr < P.Image.size())
      P.Symbols[*Name] = *Addr;
  }
  return std::move(P);
}

static std::optional<unsigned> hexByte(StringRef Text, size_t At) {
  unsigned Value;
  if (At + 2 > Text.size() || Text.substr(At, 2).getAsInteger(16, Value))
    return std::nullopt;
  return Value;
}

static Expected<Program> loadIntelHex(StringRef Text) {
  Program P;
  unsigned LineNo = 0;
  uint32_t Base = 0;
  for (StringRef Line : split(Text, '\n')) {
    ++LineNo;
    Line = Line.trim();
    if (Line.empty())
      continue;
    auto Malformed = [&] {
      return failure("not-z80",
                     "malformed Intel HEX record on line " + Twine(LineNo));
    };
    if (Line[0] != ':' || Line.size() < 11 || Line.size() % 2 == 0)
      return Malformed();

    std::vector<uint8_t> Bytes;
    for (size_t At = 1; At < Line.size(); At += 2) {
      std::optional<unsigned> Byte = hexByte(Line, At);
      if (!Byte)
        return Malformed();
      Bytes.push_back(*Byte);
    }
    unsigned Count = Bytes[0];
    if (Bytes.size() != Count + 5)
      return Malformed();
    uint8_t Sum = 0;
    for (uint8_t Byte : Bytes)
      Sum += Byte;
    if (Sum != 0)
      return Malformed();

    uint16_t Addr = Bytes[1] << 8 | Bytes[2];
    ArrayRef<uint8_t> Payload = ArrayRef(Bytes).slice(4, Count);
    switch (Bytes[3]) {
    case 0x00:
      if (uint64_t(Base) + Addr + Count > P.Image.size())
        return failure("too-large",
                       "the program does not fit in the 64 KB address space");
      std::memcpy(&P.Image[Base + Addr], Payload.data(), Count);
      break;
    case 0x01:
      return std::move(P);
    case 0x02:
    case 0x04:
      if (Count != 2)
        return Malformed();
      Base = (Payload[0] << 8 | Payload[1]) << (Bytes[3] == 0x02 ? 4 : 16);
      break;
    case 0x03:
    case 0x05: {
      if (Count != 4)
        return Malformed();
      uint32_t Start = Bytes[3] == 0x03
                           ? (Payload[0] << 8 | Payload[1]) * 16 +
                                 (Payload[2] << 8 | Payload[3])
                           : uint32_t(Payload[0]) << 24 | Payload[1] << 16 |
                                 Payload[2] << 8 | Payload[3];
      if (Start >= P.Image.size())
        return failure("not-z80",
                       "the start address is outside the address space");
      P.Entry = Start;
      break;
    }
    default:
      return Malformed();
    }
  }
  return failure("not-z80", "the Intel HEX file has no end-of-file record");
}

Expected<Program> llvm::z80run::loadProgram(StringRef Path) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> BufferOrErr =
      MemoryBuffer::getFile(Path, /*IsText=*/false,
                            /*RequiresNullTerminator=*/false);
  if (!BufferOrErr)
    return failure("no-input", BufferOrErr.getError().message());
  MemoryBufferRef Buffer = (*BufferOrErr)->getMemBufferRef();
  StringRef Data = Buffer.getBuffer();
  if (Data.starts_with(ELF::ElfMagic))
    return loadELF(Buffer);
  if (Data.ltrim().starts_with(":"))
    return loadIntelHex(Data);
  return failure("not-z80", "neither ELF nor Intel HEX");
}
