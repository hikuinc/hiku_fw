server.log(format("Agent started, external URL=%s", http.agenturl()));

// Table of responses to the getImpeeId request
gImpeeIdResponses <- [];

//**********************************************************************
// Handle incoming requests to my external agent URL.  
// Responses are queued and serviced when the device responds. 
// TODO: support multiple response types in the queue. Also right now
// it assumes the same response for all requests of the same type. 
http.onrequest(function (request, res)
{
    // Handle supported requests
    if (request.path == "/getImpeeId") {
        gImpeeIdResponses.push(res);
        device.send("request", "getImpeeId");
    } else {
      server.log(format("AGENT Error: unexpected path %s", request.path));
      res.send(400, format("unexpected path %s", request.path));
  }
});

//**********************************************************************
// Receive impee ID from the device and send to the external requestor 
device.on(("impeeId"), function(id) {
    foreach(res in gImpeeIdResponses) {
        res.send(200, id);
    }
    gImpeeIdResponses = [];
});

//**********************************************************************
// Receive a beep packet from the device and handles it 
device.on(("uploadBeep"), function(data) {
    // DEBUG: print the contents of the table to the log
    //dumpTable(data);
    
    sendBeepToHikuServer(data);  
});

//**********************************************************************
// Send the beep packet from the agent to the server
function sendBeepToHikuServer(data) {
    // Encode the audio data as base64
    data.audiodata = http.base64encode(data.audiodata);
    
    // DEBUG log URL-encoded data
    server.log(http.urlencode(data));
    
    // Send the data to the server
    local host = "http://srv2.hiku.us/" // Official server
    //local host = "http://bobert.net:4444/" // Bob's test server
    //local host = "http://www.unalignedaccess.net:9900/" // Harvey's test server
    local res = http.post(host+"scanner_1/imp_beep", 
              {"Content-Type": "application/x-www-form-urlencoded" }, 
              http.urlencode(data)
             ).sendsync();
    
    // Tell the device the result
    local result = {
        httpCode=res.statuscode,
        data=res.body
    }
    local serStr = http.jsonencode(result);
    device.send("uploadCompleted", serStr);
}

//**********************************************************************
// Print the contents of a table
function dumpTable(data)
{
    foreach (k, v in data)
    {
        if (typeof v == "table")
        {
            server.log(">>>");
            dumpTable(v);
            server.log("<<<");
        }
        else
        {
            if (v == null)
            {
                v = "(null)"
            }
            server.log(k.tostring() + "=" + v.tostring());
        }
    }
}

