# Prelude Terminal (reTerminal E1001 firmware)

BLE-controlled e-paper terminal for the Seeed reTerminal E1001.

## Build / flash / monitor

    pio run -e reterminal_e1001
    pio run -e reterminal_e1001 -t upload
    pio device monitor -e reterminal_e1001

## Host tests

    pio test -e native
