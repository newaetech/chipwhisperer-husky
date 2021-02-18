//////////////////////////////////////////////////////////////////////////////////
// Company: NewAE
// Engineer: Jean-Pierre Thibault
// 
// Create Date: 
// Design Name: 
// Module Name: 
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////

`define REG_TIMESTAMPS_DISABLE       5'h00
`define REG_PATTERN                  5'h01
`define REG_PATTERN_MASK             5'h02
`define REG_PATTERN_BYTES            5'h03
`define REG_USB_SPEED                5'h04
`define REG_USB_AUTO_DEFAULTS        5'h05
`define REG_CAPTURE_DELAY            5'h06
`define REG_USB_AUTO_WAIT1           5'h07
`define REG_USB_AUTO_WAIT2           5'h08
`define REG_STAT_PATTERN             5'h09
`define REG_STAT_MATCH               5'h0a


// FIFO bitfields:
//                   31 30 29 28 27 26 25 24   23 22 21 20 19 18 17 16   15 14 13 12 11 10  9  8   7  6  5  4  3  2  1  0
//                 +------------------------++------------------+-----++------------------------++---------------+--------+
// data command:   |         zeros          ||    FIFO status   | 0 0 ||    USB  data  byte     ||  USB status   |  time  |
//                 +------------------------++------------------+-----++------------------------++---------------+--------+
// stat command:   |         zeros          ||    FIFO status   | 0 1 ||        zeros           ||  USB status   |  time  |
//                 +------------------------++------------------+-----++------------------------++---------------+--------+
// time command:   |         zeros          ||    FIFO status   | 1 0 ||              long     time      stamp            |
//                 +------------------------++------------------+-----++------------------------++------------------------+
// stream command: |         zeros          ||    FIFO status   | 1 1 ||      stream status     ||          zeros         |
//                 +------------------------++------------------+-----++------------------------++------------------------+
`define FE_FIFO_TIME_START 0
`define FE_FIFO_SHORTTIME_LEN 3
`define FE_FIFO_FULLTIME_LEN 16

`define FE_FIFO_USB_STATUS_BITS_START 3
`define FE_FIFO_USB_STATUS_BITS_LEN 5
`define FE_FIFO_RXACTIVE_BIT 3
`define FE_FIFO_RXERROR_BIT 4
`define FE_FIFO_SESSVLD_BIT 5
`define FE_FIFO_SESSEND_BIT 6
`define FE_FIFO_VBUSVLD_BIT 7

// and this is where those FIFO status bits show up in the FIFO read,
// as opposed to register read:
// (avoiding refering to `FIFO_STAT_* to keep our Python parser stupid simple)
`define FE_FIFO_STAT_START 18
`define FE_FIFO_STAT_OFFSET 2
`define FE_FIFO_STAT_EMPTY 2
`define FE_FIFO_STAT_UNDERFLOW 3
`define FE_FIFO_STAT_EMPTY_THRESHOLD 4
`define FE_FIFO_STAT_FULL 5
`define FE_FIFO_STAT_OVERFLOW_BLOCKED 6
`define FE_FIFO_STAT_CAPTURE_DONE 7

// USB speed definitions
`define USB_SPEED_AUTO 0
`define USB_SPEED_LS 1
`define USB_SPEED_FS 2
`define USB_SPEED_HS 3

