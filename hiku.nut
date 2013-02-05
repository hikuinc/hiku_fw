// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Globals
gAudioOut <- OutputPort("Audio", "string");  // Test audio output
gScannerOutput <- "";  // String containing barcode data
gButtonState <- "UP";  // Button state ("down", "up") 
                       // TODO: use enumeration/const
gNumBuffersReady <- 0;    // Number of buffers recorded in latest button press

// Audio buffers   (TODO: review)
// TODO: doc says A-law is signed, but I see 0-256. 
// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. What size is optimal?
//buf1 <- blob(2000);
//buf2 <- blob(2000);
//buf3 <- blob(2000);
buf1 <- blob(10);
buf2 <- blob(10);
buf3 <- blob(10);


//****************************************************************************
// Agent callback: do_beep: beep in response to callback
agent.on(("do_beep"), function(msg) {
    server.log(format("in do_beep, msg=%d", msg));
    beep();
    beep();
});



//****************************************************************************
//TODO: review and cleanup
//TODO: instead of reconfiguring, just change duty cycle to zero via
//      pin.write(0). Does this have power impact? 
//TODO: minimize impact of busy wait -- slows responsiveness. Can do 
//      async and maintain sound?
//TODO: support different sounds for boot, scan success, scan fail, 
//      product found, product not found (via table of ([1/Hz],[duration]))
function beep() 
{
    hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(DIGITAL_OUT);
    hardware.pin7.write(0);
}


//****************************************************************************
// UART12 callback, called whenever there is data from scanner
// Reads the bytes, and detects and handles a full barcode string
function scannerCallback()
{
    // Read the first byte
    local data = hardware.uart12.read();
    while (data != -1)  
    {
        //server.log("char " + data + " \"" + data.tochar() + "\"");

        // Handle the data
        switch (data) 
        {
            case '\n':
                // Scan complete. Discard the line ending,
                // do something with the string, and reset state.
                server.log("Code: \"" + gScannerOutput + "\" (" + 
                           gScannerOutput.len() + " chars)");
                beep();
                
                // Reset for next scan
                gScannerOutput = "";
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
        data = hardware.uart12.read();
    } 

    //server.log("EXITING CALLBACK");
}


//****************************************************************************
// Button handler callback 
// Not a true interrupt handler, this cannot interrupt other Squirrel code. 
// The event is queued and the callback is called next time the Imp is idle. 
function buttonCallback()
{
    // Sample the button multiple times to debounce. Total time 
    // taken is (numSamples-1)*sleepSecs
    const numSamples = 4;
    const sleepSecs = 0.003; 
    local buttonState = hardware.pin9.read()
    for (local i=1;i<numSamples;i++)
    {
        buttonState += hardware.pin9.read()
        imp.sleep(sleepSecs)
    }

    // Handle the button state transition
    switch(buttonState) 
    {
        case 0:
            // Button in held state
            if (gButtonState == "UP")
            {
                gButtonState = "DOWN";
                server.log("Button state change: " + gButtonState);

                // Trigger the scanner
                hardware.pin8.write(0);

                // Trigger the mic recording
                gNumBuffersReady = 0; 
                hardware.sampler.start();
            }
            break;
        case numSamples:
            // Button in released state
            if (gButtonState == "DOWN")
            {
                gButtonState = "UP";

                // Release scanner trigger
                hardware.pin8.write(1); 

                // Stop mic recording
                hardware.sampler.stop();

                server.log("Button state change: " + gButtonState);
            }
            break;
        default:
            // Button is in transition (not settled)
            //server.log("Bouncing! " + buttonState);
            break;
    }
}


//****************************************************************************
// Called when an audio sampler buffer is ready.  It is called 
// ((sample rate * bytes per sample)/buffer size) times per second.  
// So for 16 kHz sampling of 8-bit A law and 2000 byte buffers, 
// it is called 8x/sec. 
function samplerCallback(buffer, length)
{
    gNumBuffersReady++;
    server.log("SAMPLER CALLBACK: size " + length + " num " + gNumBuffersReady);
    if (length <= 0)
    {
        server.log("Audio sampler buffer overrun!!!!!!");
    }
    else 
    {
        // Output the buffer 
        //gAudioOut.set(buffer);  TODO: remove gAudioOut
        //agent.send("sendAudioBuffer", format("length=%d", length));
        server.log("START sending audio");
        agent.send("sendAudioBuffer", buffer);
        server.log("END sending audio");
        
        // Print buffer to the log
        /*
        local str = "";
        local i = 0;
        buffer.seek(0);
        while(!buffer.eos() && i < length)
        {
            str += buffer.readn('c').tostring(); // read signed 8 bit
            //str += buffer.readn('w').tostring(); // read unsigned 16 bit
            //str += buffer.readn('s').tostring(); // read signed 16 bit
            str += " ";
            i++;  // One byte for 8 bit data (TODO bulletproof)
            //i+=2;  // Two bytes for 16 bit data (TODO bulletproof)
        }
        server.log(str);
        */

        // Debug print
        //gAudioOut.set(format("Got %d samples", gNumBuffersReady));
    }
}


//****************************************************************************
function init()
{
    //imp.configure("hiku", [], [gAudioOut])
    imp.configure("hiku", [], [])
    imp.setpowersave(false);

    // Pin configuration
    hardware.pin9.configure(DIGITAL_IN_PULLUP, buttonCallback); // Button
    hardware.pin8.configure(DIGITAL_OUT); // Scanner trigger

    // Microphone sampler config
    hardware.sampler.configure(hardware.pin5, 1, [buf1, buf2, buf3], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 
                               //samplerCallback);
                               //samplerCallback, NORMALISE); 
    

    // Scanner UART config 
    hardware.configure(UART_12); 
    hardware.uart12.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    server.log(format("Free memory at boot: %d", imp.getmemoryfree()));
    beep(); 
}


//****************************************************************************
// main
init();

