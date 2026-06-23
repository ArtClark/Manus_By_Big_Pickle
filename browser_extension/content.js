chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
    const action = msg.action || msg.cmd;
    switch (action) {
        case "getHTML":
            sendResponse({ html: document.documentElement.outerHTML });
            break;

        case "getText":
            sendResponse({ text: document.body.innerText });
            break;

        case "getLinks":
            const links = Array.from(document.querySelectorAll("a")).map((a, i) => ({
                index: i,
                text: a.innerText.trim().substring(0, 100),
                href: a.href
            }));
            sendResponse({ links, count: links.length });
            break;

        case "clickLink": {
            const idx = msg.index;
            const links = document.querySelectorAll("a");
            if (idx >= 0 && idx < links.length) {
                links[idx].click();
                sendResponse({ clicked: idx, href: links[idx].href });
            } else {
                sendResponse({ error: `Index ${idx} out of range (0-${links.length - 1})` });
            }
            break;
        }

        case "clickElement": {
            const sel = msg.selector;
            if (!sel) { sendResponse({ error: "No selector provided" }); break; }
            const el = document.querySelector(sel);
            if (el) { el.click(); sendResponse({ clicked: sel }); }
            else { sendResponse({ error: `Element not found: ${sel}` }); }
            break;
        }

        case "injectJS":
            try {
                const result = eval(msg.code);
                sendResponse({ result: String(result) });
            } catch (e) {
                sendResponse({ error: e.message });
            }
            break;

        case "getTitle":
            sendResponse({ title: document.title });
            break;

        case "getUrl":
            sendResponse({ url: location.href });
            break;

        default:
            sendResponse({ error: `Unknown action: ${action}` });
    }
});
