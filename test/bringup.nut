// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

server.log("[[Super short bring-up code]]");

gPin1HandlerSet <- false;


//**********************************************************************
//**********************************************************************
// Piezo class
class Piezo
{
    // The hardware pin controlling the piezo 
    pin = null;

    // In Squirrel, if you initialize a member array or table, all
    // instances will point to the same one.  So init in the constructor.
    tonesParamsList = {};

    // State for playing tones asynchronously
    currentToneIdx = 0;
    currentTone = null;

    // Audio generation constants
    static noteB4 = 0.002025; // 494 Hz 
    static noteE5 = 0.001517; // 659 Hz
    static noteE6 = 0.000759; // 1318 Hz
    static dc = 0.5; // 50% duty cycle, maximum power for a piezo
    static longTone = 0.2; // duration in seconds
    static shortTone = 0.15; // duration in seconds

    //**********************************************************
    constructor(hwPin)
    {
        pin = hwPin;

        tonesParamsList = {
            // [[period, duty cycle, duration], ...]
            "success": [[noteE5, dc, longTone], [noteE6, dc, shortTone]],
            "failure": [[noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone]],
            "timeout": [[noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone]],
            "sleep":   [[noteE5, dc, longTone], [noteB4, dc, shortTone]],
            "startup": [[noteB4, dc, longTone], [noteE5, dc, shortTone]],
        };
    }

    //**********************************************************
    // Play a tone (a set of notes).  Defaults to asynchronous
    // playback, but also supports synchronous via busy waits
    function playSound(tone = "success", async = true) 
    {
        // Handle invalid tone values
        if (!(tone in tonesParamsList))
        {
            server.log("Error: unknown tone");
            return;
        }

        server.log("Playing sound " + tone);

        if (async)
        {
            // Play the first note
            local params = tonesParamsList[tone][0];
            pin.configure(PWM_OUT, params[0], params[1]);
            
            // Play the next note after the specified delay
            currentTone = tone;
            currentToneIdx = 1;
            imp.wakeup(params[2], _continueSound.bindenv(this));
        }
        else 
        {
            // Play synchronously
            foreach (params in tonesParamsList[tone])
            {
                pin.configure(PWM_OUT, params[0], params[1]);
                imp.sleep(params[2]);
            }
            pin.write(0);
        }
    }
        
    //**********************************************************
    // Continue playing an asynchronous sound. This is the 
    // callback that plays all notes after the first. 
    function _continueSound()
    {
        // Turn off the previous note
        pin.write(0);

        // This happens when playing more than one tone concurrently, 
        // which can happen if you scan again before the first tone
        // finishes.  Long term solution is to create a queue of notes
        // to play. 
        if (currentTone == null)
        {
            server.log("ERROR: tried to play null tone");
            return;
        }

        // Play the next note, if any
        if (tonesParamsList[currentTone].len() > currentToneIdx)
        {
            local params = tonesParamsList[currentTone][currentToneIdx];
            pin.configure(PWM_OUT, params[0], params[1]);

            currentToneIdx++;
            imp.wakeup(params[2], _continueSound.bindenv(this));
        }
        else 
        {
            currentToneIdx = 0;
            currentTone = null;
        }
    }
}


//**********************************************************************
//**********************************************************************
// IO Expander Class for SX1508
class IoExpanderDevice
{
    i2cPort = null;
    i2cAddress = null;
    irqCallbacks = array(8); //BP ADDED.  TODO move out? 

    constructor(port, address)
    {
        if(port == I2C_12)
        {
            // Configure I2C bus on pins 1 & 2
            hardware.configure(I2C_12);
            i2cPort = hardware.i2c12;
        }
        else if(port == I2C_89)
        {
            // Configure I2C bus on pins 8 & 9
            hardware.configure(I2C_89);
            i2cPort = hardware.i2c89;
        }
        else
        {
            server.log(format("Invalid I2C port specified: %c", port));
        }

        i2cAddress = address << 1;

        // BP ADDED
        // TODO Move configure, as this would get called for each 
        // of multiple IOExpanders
        if (!gPin1HandlerSet) {
            server.log("--------Setting interrupt handler for pin1--------");
            hwInterrupt.configure(DIGITAL_IN_WAKEUP, getIrqSources.bindenv(this));
            gPin1HandlerSet = true;
        }
    }

    // Read a byte
    function read(register)
    {
        local data = i2cPort.read(i2cAddress, format("%c", register), 1);
        if(data == null)
        {
            server.log("I2C read failure");
            // TODO: this should return null, right??? Do better handling.
            return -1;
        }

        return data[0];
    }

    // Write a byte
    function write(register, data)
    {
        i2cPort.write(i2cAddress, format("%c%c", register, data));
    }

    // Write a bit to a register
    function writeBit(register, bitn, level)
    {
        local value = read(register);
        value = (level == 0)?(value & ~(1<<bitn)):(value | (1<<bitn));
        write(register, value);
    }

    // Write a masked bit pattern
    function writeMasked(register, data, mask)
    {
        local value = read(register);
        value = (value & ~mask) | (data & mask);
        write(register, value);
    }

    // Get a GPIO input pin level
    function getPin(gpio)
    {
        return (read(0x08)&(1<<(gpio&7)))?1:0;
    }

    // Set a GPIO level
    function setPin(gpio, level)
    {
        writeBit(0x08, gpio&7, level?1:0);
    }

    // Set a GPIO direction
    function setDir(gpio, input)
    {
        writeBit(0x07, gpio&7, input?1:0);
    }

    // Set a GPIO internal pull up
    function setPullUp(gpio, enable)
    {
        writeBit(0x03, gpio&7, enable);
    }

    // Set GPIO interrupt mask
    // "0" means disable interrupt, "1" means enable (opposite of datasheet)
    function setIrqMask(gpio, enable)
    {
        writeBit(0x09, gpio&7, enable?0:1); 
    }

    // Set GPIO interrupt edges
    function setIrqEdges(gpio, rising, falling)
    {
        local addr = 0x0B - (gpio>>2);
        local mask = 0x03 << ((gpio&3)<<1);
        local data = (2*falling + rising) << ((gpio&3)<<1);
        writeMasked(addr, data, mask);
    }

    // Clear an interrupt
    function clearIrq(gpio)
    {
        writeBit(0x0C, gpio&7, 1);
    }

    // BP ADDED
    function setIrqCallback(pin, func){
        irqCallbacks[pin] = func;
    }

    // BP ADDED
    function clearIrqCallback(pin){
        irqCallbacks[pin] = null;
    }

    // BP ADDED
    // TODO: why does the get function also handle interrupts??
    // TODO: do we need to return an array of active interrupts? 
    function getIrqSources(){
        server.log("INTERRUPT!");

        // Get the active interrupt sources
        local regInterruptSource = read(0x0C);

        // Convert this into an array of pins who have active interrupts
        local irqSources = array(8);

        local i = 0;
        for(local z=1; z<=0xFF; z=z<<1){
            irqSources[i] = ((regInterruptSource & z) == z);
            i++;
        }

        // Call the interrupt handlers for all active interrupts
        for(local pin=0; pin < 8; pin++){
            if(irqSources[pin]){
                server.log(format("-Calling irq callback for pin %d", pin));
                irqCallbacks[pin]();
                // clearIrq(pin); by default interrupts auto-clear on RegRead
            }
        }

        server.log("CLEARING ALL INTERRUPTS");
        write(0x0C, 0xFF); // TODO remove; clear all interrupts

        //Return array of the IO pins who have active interrupts
        return irqSources;
    }

    // Print all registers (well, the first 18)
    function printExpanderRegs()
    {
        local label = "";
        for (local i=0; i<0x11; i++)
        {
            switch (i) {
                case 0x03:
                    label = "RegPullUp";
                    break;
                case 0x07:
                    label = "RegDir";
                    break;
                case 0x08:
                    label = "RegData";
                    break;
                case 0x09:
                    label = "RegInterruptMask";
                    break;
                case 0x0A:
                    label = "RegSenseHigh";
                    break;
                case 0x0B:
                    label = "RegSenseLow";
                    break;
                case 0x0C:
                    label = "RegInterruptSource";
                    break;
                case 0x0D:
                    label = "RegEventStatus";
                    break;
                case 0x10:
                    label = "RegMisc";
                    break;
                default:
                    label = "";
                    break;
            }
            printRegister(i, label);
        }
    }

    // Print register contents in hex and binary
    function printRegister(regIdx, label="")
    {
        local reg = read(regIdx);
        local regStr = "";
        for(local j=7; j>=0; j--)
        {
            if (reg & 1<<j) 
            {
                regStr += "1";
            }
            else
            {
                regStr += "0";
            }

            if (j==4) 
            {
                regStr += " ";
            }
        }

        server.log(format("REG 0x%2X=%s (0x%2X) %s", regIdx, regStr, 
                          reg, label));
    }
}

//**********************************************************************
class PushButton extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment

    constructor(port, address, btnPin)
    {
        base.constructor(port, address);

        // Save assignments
        pin = btnPin;

        // Set event handler for IRQ
        setIrqCallback(btnPin, buttonHandler.bindenv(this));

        // Configure pin as input, IRQ on both edges
        setDir(pin, 1); // set as input
        setPullUp(pin,1); // enable pullup
        setIrqMask(pin, 1); // enable IRQ
        setIrqEdges(pin, 1, 1); // both rising and falling

        write(0x0C, 0xFF); // TODO remove; clear all interrupts
    }

    // For hiku, we treat the class as a singleton and have one 
    // interrupt handler. We could pass in a per-instance handler.
    function buttonHandler()
    {
        local state = getPin(pin) ? "UP" : "DOWN";
        server.log(format("--PushButton buttonHandler; button %d = %s", 
                    pin, state));
    }

    function readState()
    {
        return getPin(pin);
    }
}

//**********************************************************************
function init()
{
    // Globals
    hwInterrupt <-hardware.pin1;
    gi2cPort <- hardware.i2c89;
    gAddrIoExpander <- 0x23;
    gAddrAccelerometer <- 0x38;
    gIoPinButton <- 2;

    // Configure and enable 3v3 accessory switch 
    // TODO: turn it off before sleeping
    sw3v3 <- IoExpanderDevice(I2C_89, gAddrIoExpander);
    sw3v3.setDir(4, 0); // output
    sw3v3.setPullUp(4,0); // pullup disabled
    sw3v3.setPin(4,0); // pull low to turn switch on
 
    // Init our EImp-connected peripherals
    hwPiezo <- Piezo(hardware.pin5);
    hwPiezo.pin.configure(DIGITAL_OUT);
    hwPiezo.pin.write(0); // Turn off piezo by default
    
    // Create our IoExpanderDevice-based peripherals
    hwButton <- PushButton(I2C_89, gAddrIoExpander, gIoPinButton);
    
    hwButton.printExpanderRegs();

    // Accelerometer
    local out = gi2cPort.read(gAddrAccelerometer, format("%c", 0x0F), 1);
    server.log("Accelerometer WHO_AM_I = " + out);
    out = gi2cPort.read(gAddrAccelerometer, format("%c", 0x20), 1);
    server.log("Accelerometer CTRL_REG1 = " + out);
    out = gi2cPort.read(gAddrAccelerometer, format("%c", 0x29), 1);
    server.log("Accelerometer OUT_X = " + out);

    // Initialization complete notification
    //hwPiezo.playSound("startup"); 
}

init();

/*
function checkButton() {
    server.log((format("Button state = %d", hwButton.readState())));
    imp.wakeup(1, checkButton);
}

imp.wakeup(1, checkButton);
*/
