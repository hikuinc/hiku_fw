// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

// Globals
gScannerOutput <- "";   // String containing barcode data
gButtonState <- "UP";  // Button state ("down", "up")

//TODO: review
function beep() 
{
    hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(DIGITAL_OUT);
    hardware.pin7.write(0);
}


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


//TODO: review
function readButton()
{
    local but = 0;
    but = hardware.pin9.read();
    imp.sleep(0.001); //1ms sleep / sample period for the button
    but = but + hardware.pin9.read();
    if (but>1)
    {
        return 1;
    }
    else
    {
        // Button pressed
        return 0;
    }
}


// Button handler callback. 
// Not a true interrupt handler, this cannot interrupt other Squirrel code. 
// The event is queued and the callback is called next time the Imp is idle. 
function buttonCallback()
{
    // Sample the button twice in x ms
    local buttonState = hardware.pin9.read()
    imp.sleep(0.001)  // In seconds
    buttonState += hardware.pin9.read()
    
    switch(buttonState) 
    {
        case 0:
            // Button in held state
            if (gButtonState == "UP")
            {
                gButtonState = "DOWN";
                server.log("Button state change: " + gButtonState);
                hardware.pin8.write(0); // Trigger the scanner
            }
            break;
        case 2:
            // Button in released state
            if (gButtonState == "DOWN")
            {
                gButtonState = "UP";
                server.log("Button state change: " + gButtonState);
                hardware.pin8.write(1); // Release scanner trigger
            }
            break;
        default:
            // Button is in transition (not settled)
            break;
    }
}


// TODO: replace with button event handler
function loop()
{
/*
    if (readButton())
    {
        hardware.pin8.write(1);
    }
    else
    {
        hardware.pin8.write(0); // trigger the scanner
    }
    imp.wakeup(0.01, loop);
*/
}


function init()
{
    imp.configure("hiku", [], [])
    imp.setpowersave(false);

    // Pin configuration
    hardware.pin9.configure(DIGITAL_IN_PULLUP, buttonCallback); // Button
    hardware.pin8.configure(DIGITAL_OUT); // Scanner trigger
    // TODO: why configure pin 5 now, if we change to PWM later?
    hardware.pin5.configure(DIGITAL_OUT); // Piezo  

    hardware.configure(UART_12); // RX-from-scanner (pin 2)
    hardware.uart12.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    // TODO: change boot sound
    beep(); 
}


init();
loop();
