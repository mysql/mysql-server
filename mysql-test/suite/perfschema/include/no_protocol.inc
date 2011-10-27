# Tests for the performance schema

# The file with expected results fits only to a run without
# ps-protocol/sp-protocol/cursor-protocol/view-protocol.
if (`SELECT $PS_PROTOCOL + $SP_PROTOCOL + $CURSOR_PROTOCOL
            + $VIEW_PROTOCOL > 0`)
{
   --skip Test requires: ps-protocol/sp-protocol/cursor-protocol/view-protocol disabled
}

