# PPB 1.1.0-beta.3

## New functionality

- Adds FU mode **Приём + передача (RX+TX)** via a third radio button.
- FU command code `0x02` with `sign=0x01`, sent as one eight-byte frame.
- Preserves legacy RX `0x00` and TX `0x01` command numbers.
- Transfers all FU parameter fields: `fu_period` and `fu_data[0..2]` using the existing duration/duty-cycle encoding.
- Supports bridge ACK for `0x02` (address, echoed command, success/error).
- Adds a byte-level FU protocol test to Windows installer CI.

## MCU / bridge developer contract

See [docs/FU_RX_TX_PROTOCOL.md](docs/FU_RX_TX_PROTOCOL.md) for the byte layout and sample frames.

**Compatibility requirement:** The PC software can emit `0x02`, but the MCU must implement it and the bridge must accept/forward it. There has been **no physical hardware validation** of simultaneous RX/TX, so this is a beta test release only.

## Validation

CI compiles Qt/C++, runs focused FU protocol tests, builds the NSIS installer, silently installs it, and launches the installed GUI in `--test` mode. It does not verify live PPB, AKIP or CH375 operation.
