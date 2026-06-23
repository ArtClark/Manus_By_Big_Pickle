let port = null;
const injectedTabs = new Set();

function ensureContent(tabId, callback) {
    if (injectedTabs.has(tabId)) { callback(); return; }
    chrome.scripting.executeScript({
        target: { tabId },
        files: ["content.js"]
    }).then(() => {
        injectedTabs.add(tabId);
        callback();
    }).catch(() => {
        callback(); // might already be injected via page reload
    });
}

function handleMessage(msg) {
    const action = msg.action || msg.cmd;

    // captureTab is handled in background (needs tabs permission)
    if (action === "captureTab") {
        chrome.tabs.captureVisibleTab(null, { format: "png" }, (dataUrl) => {
            if (chrome.runtime.lastError || !dataUrl) {
                if (port) port.postMessage({ error: "captureVisibleTab failed" });
                return;
            }
            if (port) port.postMessage({ dataUrl: dataUrl });
        });
        return;
    }

    // All other actions need content.js in the tab
    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
        if (tabs.length === 0) {
            if (port) port.postMessage({ error: "No active tab" });
            return;
        }
        ensureContent(tabs[0].id, () => {
            chrome.tabs.sendMessage(tabs[0].id, msg, (resp) => {
                if (chrome.runtime.lastError) {
                    // Content script not responding — inject fresh
                    injectedTabs.delete(tabs[0].id);
                    ensureContent(tabs[0].id, () => {
                        chrome.tabs.sendMessage(tabs[0].id, msg, (resp2) => {
                            if (port) port.postMessage(resp2 || { error: "Content script not available" });
                        });
                    });
                    return;
                }
                if (port) port.postMessage(resp || {});
            });
        });
    });
}

function connectNative() {
    try {
        port = chrome.runtime.connectNative("com.manus.bridge");
        port.onMessage.addListener(handleMessage);
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

// Clear injected set on tab removal
chrome.tabs.onRemoved.addListener((tabId) => { injectedTabs.delete(tabId); });
