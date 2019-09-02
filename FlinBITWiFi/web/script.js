/* --- Tokeniser --- */

var IsWhitespace = function(char)
{
    return char == ' ' || char == '\t' || char == '\n' || char == '\r'
}

function Tokeniser(str)
{
    this.rest = str || "";
};

Tokeniser.prototype.next = function()
{
    var rtn = "";
    var i = 0;
    for (; i < this.rest.length && IsWhitespace(this.rest[i]); i++);
    for (; i < this.rest.length && !IsWhitespace(this.rest[i]); i++)
        rtn += this.rest[i];
    if (i < this.rest.length)
        this.rest = this.rest.substring(i);
    else
        this.rest = "";
    return rtn;
};

Tokeniser.prototype.nextLower = function()
{
    return this.next().toLowerCase();
}

/* --- Web Serial --- */

function WebSerial(address, onMessage, onOpen, onClose, onError)
{
    this.address = address || "";
    this.socket = null;
    this.onMessage = onMessage || function(event){};
    this.onOpen = onOpen || function(event){};
    this.onClose = onClose || function(event){};
    this.onError = onError || function(event){};

    if (this.address != null && this.address != undefined && this.address != "")
    {
        this.reconnect();
    }
};

WebSerial.prototype.reconnect = function()
{
    this.socket = new WebSocket(this.address);
    this.socket.onmessage = this.onMessage;
    this.socket.onopen = this.onOpen;
    this.socket.onerror = this.onError;
    this.socket.onclose = this.onClose;
};

WebSerial.prototype.connect = function(address)
{
    this.address = address;
    this.reconnect();
};

WebSerial.prototype.close = function()
{
    if (this.socket)
    {
        this.socket.close();
        this.socket = null;
    }
};

WebSerial.prototype.print = function(message)
{
    this.socket.send(message);
};

WebSerial.prototype.println = function(message)
{
    this.socket.send(message+'\n');
};

/* --- Global Objects --- */

var Plotter;
var PlotterIndex = -1;
var PlotterData = "0,0";
var PlotterAxis = "";

var PlotterResize = function()
{
    if (document.getElementById('serialPlotter').style.display != 'none') Plotter.resize();
}

var PlotterRefresh = function()
{
    Plotter.updateOptions({'file':PlotterData});
}

var Serial = new WebSerial(
    /* addresss */
    'ws://192.168.1.1:81',
    /* onMessage */
    function(event) { MessageParser(event.data); },
    /* onOpen */
    function(event) { document.getElementById('connectionClosed').style.display = 'none'; PlotterResize(); },
    /* onClose */
    function(event) { document.getElementById('connectionClosed').style.display = 'flex'; PlotterResize(); },
    /* onError */
    function(event) {}
);

/* --- Functions --- */

var StartLogger = function()
{
    var filename = document.getElementById('serialLoggerFileName').value;
    var overwrite = document.getElementById('fileModeOverwrite').checked;

    if (overwrite === true)
        Serial.println('/logstart overwrite '+filename);
    else
        Serial.println('/logstart append '+filename);

    document.getElementById('serialLoggerStartButton').disabled = true;
}

var StopLogger = function()
{
    Serial.println('/logstop');

    document.getElementById('serialLoggerStopButton').disabled = true;
}

var MessageParser = function(message)
{
    var isCommand = false;

    if (message.length > 1 && message[0] == '/')
    {
        message = message.slice(1);
        if (message[0] != '/') { isCommand = true; }
    }

    if (isCommand)
    {
        // command
        var tokens = new Tokeniser(message);
        var command = tokens.nextLower();
        if (command == 'plotter')
        {
            command = tokens.nextLower();
            if (command == "show")
            {
                document.getElementById('serialPlotter').style.display = 'flex';
                PlotterResize();
            }
            else if (command == "hide")
            {
                document.getElementById('serialPlotter').style.display = 'none';
            }
            else if (command == "toggle")
            {
                if (document.getElementById('serialPlotter').style.display == 'none')
                    document.getElementById('serialPlotter').style.display = 'flex';
                else
                    document.getElementById('serialPlotter').style.display = 'none';
                PlotterResize();
            }
            else if (command == "clear")
            {
                PlotterIndex = -1;
                PlotterData = "";
                PlotterRefresh();
            }
        }
        else if (command == 'logger')
        {
            command = tokens.nextLower();
            if (command == "show")
            {
                document.getElementById('serialLogger').style.display = 'flex';
                PlotterResize();
            }
            else if (command == "hide")
            {
                document.getElementById('serialLogger').style.display = 'none';
                PlotterResize();
            }
            else if (command == "toggle")
            {
                if (document.getElementById('serialLogger').style.display == 'none')
                    document.getElementById('serialLogger').style.display = 'flex';
                else
                    document.getElementById('serialLogger').style.display = 'none';
                PlotterResize();
            }
            else if (command == "running")
            {
                document.getElementById('serialLoggerRunning').style.display = 'flex';
                document.getElementById('serialLoggerNotRunning').style.display = 'none';

                document.getElementById('serialLoggerStartButton').disabled = false;
                document.getElementById('serialLoggerStopButton').disabled = false;
            }
            else if (command == "notrunning")
            {
                document.getElementById('serialLoggerNotRunning').style.display = 'flex';
                document.getElementById('serialLoggerRunning').style.display = 'none';

                document.getElementById('serialLoggerStartButton').disabled = false;
                document.getElementById('serialLoggerStopButton').disabled = false;
            }
        }
        else if (command == 'set')
        {
            command = tokens.nextLower();

            if (command == 'background')
            {
                document.body.style.backgroundColor = tokens.rest;
            }
            else if (command == 'foreground')
            {
                document.body.style.color = tokens.rest;
            }
        }
    }
    else
    {
        // not a command
        if (document.getElementById('serialPlotterEnabled').checked)
        {
            if (PlotterIndex < 0)
            {
                PlotterIndex = 0;
                var axis = 0; // the last axis is intentionally ignored
                for (var i = 0; i < message.length; ++i) if (message[i] == ',') ++axis;
                for (var i = 0; i < axis; ++i) PlotterAxis += ""+i+",";
                PlotterAxis += ""+axis+"\n";
                PlotterData = PlotterAxis;
            }
            else
            {
                var maxPlotterDatas = document.getElementById("serialPlotterMax").value;
                var curPlotterDatas = 0;
                for (var i = PlotterData.length; i --> 0;)
                {
                    if (PlotterData[i] == '\n')
                        ++curPlotterDatas;
                    if (curPlotterDatas >= maxPlotterDatas)
                    {
                        PlotterData = PlotterAxis + PlotterData.substring(i + 1);
                        break;
                    }
                }
            }
            PlotterData += ""+PlotterIndex+","+message;
            PlotterIndex++;
            PlotterRefresh();
        }
    }
}

var SendMessageFromTextArea = function(id)
{
    var elem = document.getElementById(id);
    Serial.println(elem.value);
    elem.value = "";
};

var SendMessageFromElement = function(id)
{
    var elem = document.getElementById(id);
    Serial.println(elem.innerHTML);
    elem.innerHTML = "";
};

var OnLoad = function()
{
    Plotter = new Dygraph(document.getElementById('serialPlotterContainer'), PlotterData, { underlayCallback: function(canvas, area, g) {
        const greenTop = g.toDomCoords(0, 6)[1];
        const amberTop = g.toDomCoords(0, 17)[1];

        // Draw green rect
        canvas.fillStyle = "rgba(0, 255, 0, 0.25)";
        canvas.fillRect(area.x, greenTop, area.w, area.h - greenTop);

        // Draw amber rect
        canvas.fillStyle = "rgba(200, 200, 0, 0.25)";
        canvas.fillRect(area.x, amberTop, area.w, greenTop - amberTop);

        // Draw red rect
        canvas.fillStyle = "rgba(255, 0, 0, 0.25)";
        canvas.fillRect(area.x, area.y, area.w, amberTop - area.y);
    }});
};

window.addEventListener('load', OnLoad);

var OnResize = function()
{
    PlotterResize();
};

window.addEventListener('resize', OnResize);
