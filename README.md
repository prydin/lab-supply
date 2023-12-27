# Linear 0-30V, 2A lab power supply

![3d rendering](assets/3dmodel.png)

## Disclaimer

This supply has been built and tested and does work. However, I am a hobbyist doing this to learn. You are
welcome to follow along, but keep the experimental status in mind and always exercise caution when dealing
with electricity!

## Abstract

This project aims to design a high-performance, high-stability microcontroller-based lab power supply. I am
mostly doing this to learn, but also because I'm tired of cheap switchmode power supplies delivering power
riddled with RF noise.

## Principle of operation

### High voltage section

The high voltage section is very simple and consists of a regular AC inlet with a safety capacitor across it.
There is a 3A fuse in series with the hot lead and a two pole mains switch. The transformer is a 24V/4A unit
with a 12V center tap. Once rectified, the voltage reaches about 36V at my location. Mains ground is connected
to the chassis as well as the ground output terminal.

### Analog secion

32-36V AC enters the circuit at the `AC1` and `AC2` terminals and gets rectified and smoothed into approximately
36-40V DC. This voltage is then pre-regulated down to 32V through `Q3` which is gets its voltage reference the
backwards bias of `D3`. This voltage is used to power the opamps. The `CNT` input is the transformer center tap,
which should be around 15V and is used to create the +12V and +5V voltages.

The `U1D` opamp acts as a voltage regulation amplifier and expects a control voltage between 0-4.096V on its postive input. The feedback loop is tuned
to 7.32x amplification, resulting in an output voltage ranging from 0-30V. The Darlington pair at `Q3` acts
as a emitter follower current amplifier.

Current regulation is based on a high-side current sensing resistor, `R27`. The differential voltage across the
current sense resistor is amplified to a 0-2.5V signal using an INA225 current sense amplifier. This voltage is
then sent to the positivie input of `U1A`. The control circuitry feeds a 0-4.096V signal
to the `V_ISET` input, which is scaled down to 0-2.5V through a resistor network. This voltage is then fed to the
negative input of `U1A`. When the voltage from the current sense amplifier goes above the control voltage on the negative input,
the opamp will source current to Q2, which brings the base on Q3 low until the current is limited to the
set value. `Q3` is needed since we want to keep R24 relatively low to avoid voltage drop at high output currents,
while making sure `U1A` isn't sourcing or sinking too much current. `Q1` acts as a simple switch for turning on
the current limit indicator LED.

### Digital section

The digital section is based on a Adafruit ItsyBitsy 5V, which is essentially an Atmel ATMega 32u4 with some
support circuitry. This was chosen to simplify the design and keep everything as through hole. The digital
section is responsible for feeding the `V_VSET` and `V_ISET` voltages to the the analog section, as well
as measuring and presenting actual voltages and currents.

A key component is the LMZ4040DBZ voltage reference, which provides a stable 4.096V reference used by the
rest of the circuit.

The control voltages are generated by an MPC4922 12-bit DAC and fed directly to the analog section. The user
sets the desired voltage and current using two rotary encoders which can be pushed to switch between a "fine"
and "coarse" mode.

The output voltage of the supply is measured by an MCP3202 ADC through a voltage divider network to bring the
voltage down to 0-4.096V. Further overvoltage protection is offered by the `D12` clamping diode. Current is
measured in a similar way, but since that signal is in the 0-1V range, it is first amplified 4.096x by `U1B`.
This signal is also clamped to the 4.096V reference through `D10`.

![schematic](assets/schematic.png)

## Controls

### Current and voltage

Current and voltage are controlled using two rotary encoders. Pressing the knob switches between coarse and fine
mode. In coarse mode, a blinking cursor will indicate the least significant digit subject to change by turning
the knob.

### Enable/disable

The enable switch enables or disables the output. Notice that this is a "soft" disable that's simply shorting
the posisitive terminal on the voltage regulation opamp to ground. Thus, the output is NOT electrically isolated
from the rest of the circuit.

### Lock mode

When the lock switch is activated, voltage and current output are locked to their current values. The user can
still change the settings, but they are not acted upon. When the switch is flipped, the voltage and current
output is changed to the current setting. This is useful when a user is carefully dialing in new settings
without changing the output while turning the knobs. When locked mode is activated, the "->" symbol between
set and actual values on the display is changed to "LCK".

## Grounding

This power supply is designed to be floating, i.e. it is isolated from ground. If the user needs either
output (plus or minus) to be grounded, they may simply short it to ground using a ground bar or patch
wire. Since the supply uses high side current sense, accidental shorts to ground should not cause damage.

## Physical design and enclosure

### PCB design

This is my very first PCB design and there are many beginners mistakes present in the design. However, it
does work. If I ever redesign this, I would make the +36V path shorter, for example. There are also some
questionable component placements. As with any open source project, you are encouraged to improve on it!
![internals](assets/internals.png)

### Enclosure

I opted for a cheap sheet metal/plastic box that is widely available on sites like Ali Express and Amazon.
It works well, but the metal is fairly thin and tends to flex when unplugging the leads.
![enclosure](assets/front.png)

### Cooling

The TIP142 output transistor requires good cooling. I opted for a simple $9 AMD CPU cooler and bolted the
transistor and the 7805 regulator to it. I'm currently using isolating pads for the TIP142, but I'm not
entirely happy with the heat transfer. I might remove that, although I'm not sure I like to have unregulated 36V
exposed inside the case. The fan is PWM controlled using a simple temperature curve algorithm.
![cooling](assets/cooling.png)

### Rotary encoders and switches

The rotary encoders and switches (with exception of the mains switch) are all cheap Amazon products and work
well so far.
