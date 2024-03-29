/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLHARNESS_TTY_INCLUDED
#define MYSQLHARNESS_TTY_INCLUDED

#include "harness_export.h"

#include <cstdint>
#include <ostream>

#ifdef _WIN32
#include <ntverp.h>   // VER_PRODUCTBUILD
#include <windows.h>  // GetConsoleMode
#else
#include <termios.h>  // tcgetattr
#include <unistd.h>
#endif

class HARNESS_EXPORT Tty {
 public:
  using fd_type = int;
#ifdef _WIN32
  using state_type = DWORD;
#else
  using state_type = struct termios;
#endif
  static fd_type fd_from_stream(std::ostream &os);
  static fd_type fd_from_stream(std::istream &is);

  Tty(fd_type fd) : fd_{fd} {
    try {
      state_ = attrs();
      is_saved_ = true;
    } catch (const std::system_error &) {
    }
  }

  // disable copy (enable it explicitly if needed)
  Tty(const Tty &) = delete;
  Tty &operator=(const Tty &) = delete;

  /**
   * restore the state of the Tty if we changed it.
   */
  ~Tty() {
    if (is_saved_) {
      try {
        restore_attrs();
      } catch (const std::system_error &) {
      }
    }
  }

  class Flags {
   public:
#ifdef _WIN32
    class Win32 {
     public:
      class Input {
       public:
        static constexpr const size_t kEcho = ENABLE_ECHO_INPUT;
        static constexpr const size_t kExtendedFlags = ENABLE_EXTENDED_FLAGS;
        static constexpr const size_t kInsertMode = ENABLE_INSERT_MODE;
        static constexpr const size_t kLineInput = ENABLE_LINE_INPUT;
        static constexpr const size_t kMouseInput = ENABLE_MOUSE_INPUT;
        static constexpr const size_t kProcessedInput = ENABLE_PROCESSED_INPUT;
        static constexpr const size_t kQuickEditMode = ENABLE_QUICK_EDIT_MODE;
        static constexpr const size_t kWindowInput = ENABLE_WINDOW_INPUT;
#if VER_PRODUCTBUILD >= 10011
        // winsdk 10.0.14393.0 and later
        static constexpr const size_t kVirtualTerminalInput =
            ENABLE_VIRTUAL_TERMINAL_INPUT;
#endif
      };
      class Output {
       public:
        static constexpr const size_t kProcessedOutput =
            ENABLE_PROCESSED_OUTPUT;
        static constexpr const size_t kWrapAtEolOutput =
            ENABLE_WRAP_AT_EOL_OUTPUT;
#if VER_PRODUCTBUILD >= 10011
        // winsdk 10.0.14393.0 and later
        static constexpr const size_t kVirtualTerminalProcessing =
            ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        static constexpr const size_t kDisableNewlineAutoReturn =
            DISABLE_NEWLINE_AUTO_RETURN;
        static constexpr const size_t kLvbGridWorldwide =
            ENABLE_LVB_GRID_WORLDWIDE;
#endif
      };
    };
#else
    class Posix {
     public:
      class Local {
       public:
        static constexpr const size_t kGenerateSignal = ISIG;
        static constexpr const size_t kCanonicalMode = ICANON;
#ifdef XCASE
        // not on freebsd, macosx
        static constexpr const size_t kConvertCase = XCASE;
#endif
        static constexpr const size_t kEcho = ECHO;
        static constexpr const size_t kEchoWithErase = ECHOE;
        static constexpr const size_t kEchoWithKill = ECHOK;
        static constexpr const size_t kEchoWithNewline = ECHONL;
        static constexpr const size_t kEchoWithControl = ECHOCTL;
        static constexpr const size_t kEchoWithPrint = ECHOPRT;
        static constexpr const size_t kEchoWithKillEares = ECHOKE;
        // static constexpr size_t kEchoIfReading = DEFECHO;
        static constexpr const size_t kOutputFlushed = FLUSHO;
        static constexpr const size_t kNoFlush = NOFLSH;
        static constexpr const size_t kToStop = TOSTOP;
        static constexpr const size_t kPending = PENDIN;
        static constexpr const size_t kExtendedInputProcessing = IEXTEN;
      };
      class Control {
       public:
#ifdef CBAUD
        // not on freebsd, macosx
        static constexpr const size_t kBaudSpeedMask = CBAUD;
#endif
#ifdef CBAUDEX
        // not on freebsd, macosx, solaris
        static constexpr const size_t kBaudSpeedMaskExtra = CBAUDEX;
#endif
#ifdef CIBAUDEX
        // not on solaris
        static constexpr const size_t kInputBaudSpeedMaskExtra = CIBAUDEX;
#endif
        static constexpr const size_t kCharacterSizeMask = CSIZE;
        static constexpr const size_t kTwoStopBits = CSTOPB;
        static constexpr const size_t kEnableReceiver = CREAD;
        static constexpr const size_t kParityCheckGenerator = PARENB;
        static constexpr const size_t kParityOdd = PARODD;
        static constexpr const size_t kHangupOnClose = HUPCL;
        static constexpr const size_t kIgnoreControlLines = CLOCAL;
#ifdef LOBLK
        static constexpr const size_t kBlockOutputNonCurrentShellLayer = LOBLK;
#endif
#ifdef CIBAUD
        // not on freebsd, macosx
        static constexpr const size_t kInputSpeedMask = CIBAUD;
#endif
#ifdef CMSPAR
        // not on freebsd, solaris
        static constexpr const size_t kStickParity = CMSPAR;
#endif
#ifdef CRTSXOFF
        // not on solaris
        static constexpr const size_t kEnableSoftFlowControl = CRTSXOFF;
#endif
        static constexpr const size_t kEnableHardFlowControl = CRTSCTS;
      };
      class Output {
       public:
        static constexpr const size_t kOutputProcessing = OPOST;
        static constexpr const size_t kMapNewlineCarriageReturn = ONLCR;
#ifdef OLCUC
        // not on freebsd, macosx
        static constexpr const size_t kMapLowercaseUppercase = OLCUC;
#endif
        static constexpr const size_t kMapCarriageReturnNewline = OCRNL;
        static constexpr const size_t kNoOutputCarriageReturnOnColumnZero =
            ONOCR;
        static constexpr const size_t kNoOutputCarriageReturn = ONLRET;
#ifdef OFILL
        // not on freebsd
        static constexpr const size_t kSendFillCharacter = OFILL;
#endif
#ifdef OFDEL
        // not on freebsd
        static constexpr const size_t kFillCharacterIsDelete = OFDEL;
#endif
#ifdef NLDLY
        // not on freebsd
        static constexpr const size_t kNewlineDelayMask = NLDLY;
#endif
#ifdef CRDLY
        // not on freebsd
        static constexpr const size_t kCarriangeReturnDelayMask = CRDLY;
#endif
        static constexpr const size_t kHorizontalTabDelayMask = TABDLY;
#ifdef BSDLY
        // not on freebsd
        static constexpr const size_t kBackspaceDelayMask = BSDLY;
#endif
#ifdef VTDLY
        // not on freebsd
        static constexpr const size_t kVerticalTabDelayMask = VTDLY;
#endif
#ifdef FFDLY
        // not on freebsd
        static constexpr const size_t kFormfeedDelayMask = FFDLY;
#endif
      };
      class Input {
       public:
        static constexpr const size_t kIgnoreBreak = IGNBRK;
        static constexpr const size_t kBreakInt = BRKINT;
        static constexpr const size_t kIgnoreParityError = IGNPAR;
        static constexpr const size_t kParityErrorMark = PARMRK;
        static constexpr const size_t kInputParityChecking = INPCK;
        static constexpr const size_t kStripCharacter = ISTRIP;
        static constexpr const size_t kMapNewlineCarriageReturn = INLCR;
        static constexpr const size_t kIgnoreCarriageReturn = IGNCR;
        static constexpr const size_t kMapCarriageReturnNewline = ICRNL;
#ifdef IUCLC
        // not on freebsd, macosx
        static constexpr const size_t kMapUppercaseLowercase = IUCLC;
#endif
        static constexpr const size_t kStartStopOutputControl = IXON;
        static constexpr const size_t kAnyCharacterRestartOutput = IXANY;
        static constexpr const size_t kStartStopInputControl = IXOFF;
        static constexpr const size_t kEchoBellOnInputLong = IMAXBEL;
#ifdef IUTF8
        // linux only
        static constexpr const size_t kInputIsUtf8 = IUTF8;
#endif
      };
    };
#endif
  };

  std::pair<uint64_t, uint64_t> window_size() const;

  state_type attrs() const;

  void attrs(state_type &tp);

  void restore_attrs() { attrs(state_); }

  void echo(bool on);

  bool is_tty() const;

  bool ensure_vt100();

 private:
  fd_type fd_;
  state_type state_;
  bool is_saved_{false};
};

#endif
