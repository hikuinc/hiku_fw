
// Audio generation constants
const noteB4 = 0.002025 // 494 Hz 
const noteE5 = 0.001517 // 659 Hz
const noteE6 = 0.000759 // 1318 Hz
const dCycle = 0.5; // 50%, or maximum power for a piezo
const longTone = 0.2; // duration in seconds
const shortTone = 0.15; // duration in seconds

// The set of audio tones used by the device
enum Tone
{
    SUCCESS,
    FAILURE,
    SLEEP,
    STARTUP,
    NUMTONES, // dummy for bounds checking
}

// The params slots map to the Tone enum above
gToneParams <- [
    // [[period, duty cycle, duration], ...]
    [[noteE5, dCycle, longTone], [noteE6, dCycle, shortTone]],
    [[noteE6, dCycle, longTone], [noteE5, dCycle, shortTone]],
    [[noteE5, dCycle, longTone], [noteB4, dCycle, shortTone]],
    [[noteB4, dCycle, longTone], [noteE5, dCycle, shortTone]],
];


//**********************************************************************
//TODO: minimize impact of busy wait -- slows responsiveness. Can do 
//      async and maintain sound?
function beep(tone = Tone.SUCCESS) 
{
    // Handle invalid tone values
    if (tone >= Tone.NUMTONES)
    {
        server.log("Error: unknown tone");
        return;
    }

    // Play the tones
    foreach (params in gToneParams[tone])
    {
        hwPiezo.configure(PWM_OUT, params[0], params[1]);
        imp.sleep(params[2]);
    }
    hwPiezo.write(0);
}


//**********************************************************************
// Very basic Python-style range generator function 
function xrange(start, stop, step=1)
{
    for(local i=start; i<stop; i+=step)
        yield i;
    return null;
}


//**********************************************************************
// Test preconfigured tones
function testTones()
{
    for (local i = 0; i < Tone.NUMTONES; i++)
    {
        beep(i);
        imp.sleep(0.5);
    }
    beep(Tone.NUMTONES);  // Test invalid tone
}


//**********************************************************************
// Play through range of frequencies
// RESULS: 310-360Hz, 170Hz sound bad on my piezo
function testPiezoRange()
{
    local periods = xrange(0.0001, 0.005, 0.0002);
    foreach (period in periods)
    {
        server.log(format("Frequency=%4.2f Hz  (%4.4f)", 1/period, period));
        hwPiezo.configure(PWM_OUT, period, 0.5);
        imp.sleep(0.5);
        hwPiezo.write(0);
        imp.sleep(0.5);
    }
    hwPiezo.write(0);
}


//**********************************************************************
// main
imp.configure("BEEP TEST", [], []);

// Pin assignment
hwPiezo <- hardware.pin5;

// Pin configuration
hwPiezo.configure(DIGITAL_OUT);
hwPiezo.write(0); // Turn off piezo by default

// Run tests
testTones();
//testPiezoRange();

