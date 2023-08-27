let input = document.getElementById("message");
let wall = document.getElementById("wall");

setInterval(function() {
    fetch("/messages")
    .then((data) => data.text())
    .then(function(text) {
        wall.innerHTML = text;
    });
}, 500);

function send_message() {
    let message = input.value.trim();

    if (message.length == 0) {
        return;
    }

    if (message.slice(0, 8) == "/rename " && message.length > 8) {
        fetch("/rename/" + message.slice(8).replace(/\s/g,''));
        input.value = null;
        return;
    }

    fetch("/messages", {
        method: "POST",
        body: message
    });
    wall.innerHTML += "<p>Sending</p><div>" + message + "</div>",
    input.value = null;
}

input.onkeyup = function(event) {
    if (event.keyCode == 13 && !event.shiftKey) {
        send_message();
    }
}