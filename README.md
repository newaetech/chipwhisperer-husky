# ChipWhisperer-Husky

![](img/huskysmall.jpg)

This repository contains the ChipWhisperer-Husky information.

* The interface code is part of [ChipWhisperer](https://github.com/newaetech/chipwhisperer).
* The examples are part of the [ChipWhisperer-Jupyter](https://github.com/newaetech/chipwhisperer-jupyter) examples.
* The [full documentation](https://rtfm.newae.com/Capture/ChipWhisperer-Husky/) is on the [NewAE RTFM site](https://rtfm.newae.com/Capture/ChipWhisperer-Husky/).

## Building SAM3U Firmware

Navigate to `ChipWhisperer-Husky/src`, clone the naeusb submodule (`git submodule update --init naeusb`),
and build with `make -j`.

You can get a more verbose output by running make as follows: `make -j VERBOSE=TRUE`

You can program an attached Husky by running `make program`.