// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.
server.log("Bob's code is running...");


// Consts and enums
const cFirmwareVersion = "0.5.0"
const cButtonTimeout = 6;  // in seconds
const cDelayBeforeDeepSleep = 30.0;  // in seconds
const cDeepSleepDuration = 86380.0;  // in seconds (24h - 20s)

enum ButtonState
{
    BUTTON_UP,
    BUTTON_DOWN,
}

enum DeviceState
/*
                           ---> SCAN_CAPTURED ------>
                          /                          \
    IDLE ---> SCAN_RECORD ---> BUTTON_TIMEOUT -->     \
                          \                      \     \
                           -------------------> BUTTON_RELEASED ---> IDLE
*/
{
    IDLE,             // 0: Not recording or processing data
    SCAN_RECORD,      // 1: Scanning and recording audio
    SCAN_CAPTURED,    // 2: Processing scan data
    BUTTON_TIMEOUT,   // 3: Timeout limit reached while holding button
    BUTTON_RELEASED,  // 4: Button released, may have audio to send
}


// Globals
gDeviceState <- null; // Hiku device current state
gButtonState <- ButtonState.BUTTON_UP; // Button current state

gButtonTimer <- null; // Handles button-held-too-long
gDeepSleepTimer <- null; // Handles deep sleep timeouts

gScannerOutput <- ""; // String containing barcode data

gAudioBufferOverran <- false; // True if an overrun occurred
gAudioChunkCount <- 0; // Number of audio buffers (chunks) captured
gAudioBufferSize <- 1000; // Size of each audio buffer 
gAudioSampleRate <- 16000; // in Hz

// Each 1k of buffer will hold 1/16 of a second of audio, or 63ms.
// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. Filed issue with IE here: 
// http://forums.electricimp.com/discussion/780/. So keep buffers small. 
buf1 <- blob(gAudioBufferSize);
buf2 <- blob(gAudioBufferSize);
buf3 <- blob(gAudioBufferSize);
buf4 <- blob(gAudioBufferSize);


//**********************************************************************
// Agent ping-pong
agent.on(("pong"), function(msg) {
    server.log(format("in agent.on pong, msg=%s", msg));
});


//**********************************************************************
// IO Expander Class for SX1509
// TODO: change all hex codes from SX1509 to SX1508!!
class IoExpander
{
    i2cPort = null;
    i2cAddress = null;
    IRQ_Callbacks = array(16); //BP ADDED.  TODO: needed? 
    
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
        // TODO do we want this here?  this would get called for each 
        // of multiple IOExpanders... :( 
        hwInterrupt.configure(DIGITAL_IN_WAKEUP, getIRQSources.bindenv(this));
    }

    // Read a byte
    function read(register)
    {
        local data = i2cPort.read(i2cAddress, format("%c", register), 1);
        if(data == null)
        {
            server.log("I2C Read Failure");
            // TODO: this should return null, right???
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
 
    // Set a GPIO level
    // TODO: remove functionality for GPIOs > 8, which we don't have
    function setPin(gpio, level)
    {
        writeBit(gpio>=8?0x10:0x11, gpio&7, level?1:0);
    }
 
    // Set a GPIO direction
    function setDir(gpio, output)
    {
        writeBit(gpio>=8?0x0e:0x0f, gpio&7, output?0:1);
    }
 
    // Set a GPIO internal pull up
    function setPullUp(gpio, enable)
    {
        writeBit(gpio>=8?0x06:0x07, gpio&7, enable);
    }
 
    // Set GPIO interrupt mask
    function setIrqMask(gpio, enable)
    {
        writeBit(gpio>=8?0x12:0x13, gpio&7, enable);
    }
 
    // Set GPIO interrupt edges
    function setIrqEdges(gpio, rising, falling)
    {
        local addr = 0x17 - (gpio>>2);
        local mask = 0x03 << ((gpio&3)<<1);
        local data = (2*falling + rising) << ((gpio&3)<<1);    
        writeMasked(addr, data, mask);
    }
 
    // Clear an interrupt
    function clearIrq(gpio)
    {
        writeBit(gpio>=8?0x18:0x19, gpio&7, 1);
    }
 
    // Get a GPIO input pin level
    function getPin(gpio)
    {
        return (read(gpio>=8?0x10:0x11)&(1<<(gpio&7)))?1:0;
    }

    // BP ADDED
    function setIRQCallBack(pin, func){
        IRQ_Callbacks[pin] = func;
    }
    
    // BP ADDED
    function clearIRQCallBack(pin){
           IRQ_Callbacks[pin] = null;
    }

    // BP ADDED
    function getIRQSources(){
        server.log("WOOHOO! Called getIRQSources!");
        // TODO: get rid of pins >=8!!!! Will not work..
        // TODO: change all hex codes from SX1509 to SX1508!!
        // 0x18=RegInterruptSourceB (Pins 15->8), 1 is an interrupt and 
        // we write a 1 to clear the interrupt
        // 0x19=RegInterruptSourceA (Pins 7->0), 1 is an interrupt and 
        // we write a 1 to clear the interrupt
        // TODO: check for read failure!!
       local sourceB = read(0x18);
       local sourceA = read(0x19);

        local irqSources = array(16);
        
        local j = 0;
        for(local z=1; z < 256; z = z<<1){
            irqSources[j] = ((sourceA & z) == z);
            irqSources[j+8] = ((sourceB & z) == z);
            j++;
        }
        //server.log(format("irqSource=%s", byteArrayString(irqSource)));
        
        //TODO: This could be in the loop above if performance becomes an issue
        for(local pin=0; pin < 16; pin++){
            if(irqSources[pin]){
                server.log("Pin = " + pin + ", sources = " + irqSources[pin]);
                IRQ_Callbacks[pin]();
                clearIRQ(pin);
            }
        }
        
        // Clear the interrupts   
        // Currently callback functions handle this
        // TODO: clear interrupts here or elsewhere
        //write(0x18, 0xFF);
        //write(0x19, 0xFF);
        return irqSources; //Array of the IO pins and who has active interrupts
    }
}


//**********************************************************************
class PushButton extends IoExpander
{
    // IO Pin assignment
    pin = null;
 
    //Callback function for interrupt
    callBack = null;
 
    constructor(port, address, btnPin, btnCall)
    {
        server.log("Constructing PushButton")
        base.constructor(port, address);
 
        // Save assignments
        pin = btnPin;
        callBack = btnCall;
 
        // Set event handler for irq
        setIRQCallBack(btnPin, irqHandler.bindenv(this))
 
        // Configure pin as input, irq on both edges
        // TODO: verify these settings
        setDir(pin, 0); // input
        setPullUp(pin,1) // enabled
        setIrqMask(pin, 0); // disabled
        setIrqEdges(pin, 1, 1); // both rising and falling

        // TODO
        local output = getPin(pin);
        server.log("Button output = " + output);
        
       server.log("PushButton Constructed")
    }
 
    function irqHandler()
    {
        local state = null;

            // Get the pin state
            state = getPin(pin)?0:1;
 
            server.log(format("Push Button %d = %d", pin, state));
            //if (callBack != null && state == 1) // BP: commented out
            if (callBack != null)
            {
                callBack();
            }
 
        // Clear the interrupt
        clearIRQ(pin);
    }
    
    function readState()
    {
        local state = getPin(pin);
 
        //server.log(format("debug %d", state));
        return state;
    }
}


//**********************************************************************
// Scanner data ready callback, called whenever there is data from scanner.
// Reads the bytes, and detects and handles a full barcode string.
function scannerCallback()
{
    server.log("Called scannerCallback!");

    // Read the first byte
    local data = hwScannerUart.read();
    while (data != -1)  
    {
        server.log("char " + data + " \"" + data.tochar() + "\"");

        // Handle the data
        switch (data) 
        {
            case '\n':
                // Scan complete. Discard the line ending,
                // upload the beep, and reset state.

                // If the scan came in late (e.g. after button up), 
                // discard it, to maintain the state machine. 
                //if (gDeviceState != DeviceState.SCAN_RECORD)
                //{
                    //server.log(format(
                               //"Got capture too late. Dropping scan %d",
                               //gDeviceState));
                    //gScannerOutput = "";
                    //return;
                //}
                //updateDeviceState(DeviceState.SCAN_CAPTURED);

                server.log("Code: \"" + gScannerOutput + "\" (" + 
                           gScannerOutput.len() + " chars)");
                /*
                   agent.send("uploadBeep", {scandata=gScannerOutput,
                                          scansize=gScannerOutput.len(),
                                          serial=hardware.getimpeeid(),
                                          instance=0, // TODO: server seems 
                                          // to need this when writing file
                                          fw_version=cFirmwareVersion,
                                          });
                                          */
                
                // Stop collecting data
                //stopScanRecord();
                break;

            case '\r':
                // Discard line endings
                break;

            default:
                // Store the character
                gScannerOutput = gScannerOutput + data.tochar();
                break;
        }

        // Read the next byte
        data = hwScannerUart.read();
    } 

    //server.log("EXITING CALLBACK");
}


//**********************************************************************
// Button handler callback 
// Not a true interrupt handler, this cannot interrupt other Squirrel code. 
// The event is queued and the callback is called next time the Imp is idle. 
function buttonCallback()
{
    server.log("WOOHOO! Called buttonCallback!");
}


//**********************************************************************
function samplerCallback() 
{
    server.log("WOOHOO! Called samplerCallback!");
}


//**********************************************************************
function interruptCallback() 
{
    server.log("WOOHOO! Called interruptCallback!");
}


//**********************************************************************
function printStartupDebugInfo()
{
    // impee ID
    server.log(format("Your impee ID: %s", hardware.getimpeeid()));

    // free memory
    server.log(format("Free memory: %d bytes", imp.getmemoryfree()));

    // hardware voltage
    server.log(format("Power supply voltage: %.2fv", hardware.voltage()));

    // Wi-Fi signal strength
    local bars = 0;
    local rssi = imp.rssi();
    if (rssi == 0)
        bars = -1;
    else if (rssi > -67)
        bars = 5;
    else if (rssi > -72)
        bars = 4;
    else if (rssi > -77)
        bars = 3; 
    else if (rssi > -82)
        bars = 2;
    else if (rssi > -87)
        bars = 1; 

    if (bars == -1)
    {
        server.log("Wi-Fi not connected");
    }
    else
    {
        server.log(format("Wi-Fi signal: %d bars (%d dBm)", bars, rssi));
    }

    // Wi-Fi base station ID
    server.log(format("Wi-Fi SSID: %s", imp.getbssid()));

    // Wi-Fi MAC address
    server.log(format("Wi-Fi MAC: %s", imp.getmacaddress()));
}


//**********************************************************************
function init()
{
    imp.configure("hiku bringup", [], []);

    // Print debug info
    printStartupDebugInfo();

    // We will always be in deep sleep unless button pressed, in which
    // case we need to be as responsive as possible. 
    imp.setpowersave(false);

    // Test sending data to agent
    agent.send("ping", "from device");

    // Pin assignment
    hwInterrupt <-hardware.pin1;
    hwMicrophone <- hardware.pin2;
    hwPiezo <- hardware.pin5;          // Will call Piezo constructor...
    hwScannerUart <- hardware.uart57;
    //hwTrigger <-hardware.pin8;        // Move to IO expander

    // Pin configuration
    // This is done in the IO Expander now (TODO: better place?)
    //hwInterrupt.configure(DIGITAL_IN_WAKEUP, interruptCallback);
    //hwTrigger.configure(DIGITAL_OUT);
    //hwTrigger.write(1); // De-assert trigger by default
    //hwPiezo.pin.configure(DIGITAL_OUT);
    //hwPiezo.pin.write(0); // Turn off piezo by default

    // Microphone sampler config
    hardware.sampler.configure(hwMicrophone, gAudioSampleRate, 
                               [buf1, buf2, buf3, buf4], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 

    // Scanner UART config 
    hardware.configure(UART_57); 
    hwScannerUart.configure(38400, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Create our IoExpander-based peripherals
    gIoExpanderAddress <- 0x23; // Use 7-bit address; TODO move to right place
    gButtonIoNum <- 2;
    hwButton <- PushButton(I2C_89, gIoExpanderAddress, gButtonIoNum, 
                           buttonCallback);


    // Accelerometer
    accel <- hardware.i2c89;
    local out = accel.read(0x38, format("%c", 0x0F), 1);
    server.log("Accel whoami = " + out);

    // 3v3 accessory switch
    sw3v3 <- IoExpander(I2C_89, gIoExpanderAddress);
    sw3v3.setDir(4, 1); // output
    sw3v3.setPullUp(4,0); // disabled
    //sw3v3.setIrqMask(pin, 0); // disabled
    //sw3v3.setIrqEdges(pin, 1, 1); // both rising and falling
    sw3v3.setPin(4,0); // pull low to turn on the 3v3 accessory switch
    local accelOut = sw3v3.getPin(4);

    server.log("accel output = " + accelOut);
    // (check R35 level)

    // Create our timers
    //gButtonTimer = CancellableTimer(cButtonTimeout, handleButtonTimeout);
    //gDeepSleepTimer = CancellableTimer(cDelayBeforeDeepSleep,
            //function() {
                //hwPiezo.playSound("sleep", false /*async*/);
                //server.sleepfor(cDeepSleepDuration); 
            //});

    // Transition to the idle state
    //updateDeviceState(DeviceState.IDLE);

    // Initialization complete notification
    //hwPiezo.playSound("startup"); 

    // If we booted with the button held down, we are most likely
    // woken up by the button, so go directly to button handling.  
    // TODO: change when we get the IO expander.  In that case we need
    // to check the interrupt source. 
    //buttonCallback();
}


//**********************************************************************
// main
init();

