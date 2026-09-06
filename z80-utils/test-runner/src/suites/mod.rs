//! The test suites. Each one discovers its own inputs, runs them and reports
//! through `crate::report`; none of them decide how output looks.

pub mod bench;
pub mod clang;
pub mod lit;
pub mod llc;
pub mod sdcc;
pub mod stdcbench;
pub mod torture;
pub mod torture_data;
pub mod utils;
