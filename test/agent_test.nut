server.log(format("Agent started, external URL=%s", http.agenturl()));
gCount <- 0;
//**********************************************************************
// Send the barcode to hiku's server
function sendBeepToHikuServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        return;
    }
    
        
    // URL-encode the whole thing
    data = http.urlencode(data);
    server.log(data);

    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            "http://199.115.118.221/scanner_1/imp_beep",
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    local res;
    local transactionTime = time();
    res = req.sendsync();
    transactionTime = time() - transactionTime;
    agentLog(format("Server transaction time: %ds", transactionTime));

    // Handle the response
    local returnString = "success-server";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // TODO DEBUG remove
        agentLog(res.body);

        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);

        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                // Possible causes: speech2text failure, unknown.
                if ("error" in body.cause)
                {
                    returnString = "failure";
                    agentLog(format("Error: server responded with %s", 
                                         body.cause.error));
                }
                // Unknown response type
                else
                {
                    returnString = "failure";
                    agentLog("Error: unexpected cause in response");
                }
            }
        }
        catch(e)
        {
            agentLog(format("Caught exception: %s", e));
            returnString = "failure";
            agentLog("Error: malformed response");
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that? 
    //device.send("uploadCompleted", returnString);
}

//**********************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    server.log(format("AGENT: %s", str));
}

function start_test()
{
	data <- {
            	scandata="00062534",
            	scansize="8",
            	serial="20000c2a6901eb2e",
                fw_version="0.5.0",
                linkedrecord="",
                audiodata="",
    		}
    sendBeepToHikuServer(data);
    
    gCount++;
    
    if( gCount % 10 == 0 )
    {
    	imp.sleep(60, start_test);
    }
    else
    {
    	imp.sleep(1, start_test);
    }
    
}

start_test();

