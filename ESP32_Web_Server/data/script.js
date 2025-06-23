var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', onload);

function onload(event) {
    initWebSocket();
}

function initWebSocket() {
    console.log('Intentando abrir una conexión WebSocket…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
    websocket.onerror = onError;
}

function onOpen(event) {
    console.log('Conexión abierta');
    // Solicitar los valores iniciales de LEDs y potenciómetros al conectar
    websocket.send("getReadings");
}

function onClose(event) {
    console.log('Conexión cerrada');
    // Intentar reconectar después de un breve delay
    setTimeout(initWebSocket, 2000);
}

function onError(event) {
    console.error('Error de WebSocket observado:', event);
}

// Función para enviar el valor del slider al ESP32
function updateSliderPWM(element) {
    var sliderNumber = element.id.charAt(element.id.length - 1); // Extrae el número del ID (ej. '1' de 'slider1')
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById("sliderValue" + sliderNumber).innerHTML = sliderValue; // Actualiza el texto de brillo localmente
    console.log(`Slider ${sliderNumber} valor: ${sliderValue}`);
    websocket.send(sliderNumber + "s" + sliderValue.toString()); // Envía el comando al ESP32 (ej. "1s50")
}

// Función que recibe el mensaje del ESP32 con las lecturas (sliders y potenciómetros)
function onMessage(event) {
    console.log("Mensaje recibido:", event.data);
    try {
        var myObj = JSON.parse(event.data);
        var keys = Object.keys(myObj);

        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            var element = document.getElementById(key);

            if (element) {
                // Si el elemento es un slider, también actualiza su valor
                if (key.startsWith("sliderValue")) {
                    // Extrae el número del slider del ID (ej. '1' de 'sliderValue1')
                    var sliderNum = key.charAt(key.length - 1);
                    document.getElementById("slider" + sliderNum).value = myObj[key];
                    element.innerHTML = myObj[key];
                } else {
                    // Para los potenciómetros u otros valores, simplemente actualiza el texto
                    element.innerHTML = myObj[key];
                }
            } else {
                console.warn(`Elemento con ID '${key}' no encontrado.`);
            }
        }
    } catch (e) {
        console.error("Error al parsear JSON o actualizar UI:", e, "Datos:", event.data);
    }
}