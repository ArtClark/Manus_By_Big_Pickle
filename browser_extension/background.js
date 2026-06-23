let port = null;
let pendingCmd = null;

function connectNative() {
    try {
        port = chrome.runtime.connectNative("com.manus.bridge");
        port.onMessage.addListener((msg) => {
            // Command from native bridge → forward to active tab
            chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
                if (tabs.length === 0) return;
                chrome.tabs.sendMessage(tabs[0].id, msg, (resp) => {
                    if (chrome.runtime.lastError) {
                        // No content script ready
                        if (port) port.postMessage({ error: "No content script available" });
                        return;
                    }
                    // Send response back to native bridge
                    if (port) port.postMessage(resp || {});
                });
            });
        });
        port.onDisconnect.addListener(() => {
            port = null;
            setTimeout(connectNative, 1000);
        });
    } catch (e) {
        port = null;
        setTimeout(connectNative, 2000);
    }
}

connectNative();
