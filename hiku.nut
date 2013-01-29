// Copyright 2013 Katmandu Technology, Inc. All rights reserved. 

// Globals
gScannerOutput <- "";

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
        //server.log(format("Button pressed", but));
        return 0;
    }
}


function scannerCallback()
{
    server.log("Got scanner callback! Data available.")
}


function init()
{
    imp.configure("hiku", [], [])
    imp.setpowersave(false);

    // Pin configuration
    hardware.pin9.configure(DIGITAL_IN_PULLUP); // Button
    hardware.pin8.configure(DIGITAL_OUT); // Scanner trigger
    // TODO: why configure pin 5 now, if we change to PWM later?
    hardware.pin5.configure(DIGITAL_OUT); // Piezo  

    hardware.configure(UART_12); // RX-from-scanner (pin 2)
    hardware.uart12.configur(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    // TODO: change sound
    beep(); 
}

// TODO: replace with button event handler
function loop()
{
    if (readButton())
    {
        hardware.pin8.write(1);
    }
    else
    {
        hardware.pin8.write(0); // trigger the scanner
    }

        b = hardware.uart12.read();
    if (b >= 0)
    {
        //server.log("got scan data!!!");
        //server.log("char: " + b);
        s = s + b.tochar();
        if ( b == '\n')
        {
            // send the upc to the server
            local val = "scan="+s;
            server.log(val);
            packetOut.set(val);
            hardware.uart12.write(val);
            beep();
            
            //now clear out 
            s = "";
            b=-1;
        } 
    } // if (b >= 0)

    imp.wakeup(0.01, loop);
}

init();
loop();
