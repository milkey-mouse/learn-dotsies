// JS is stupid
function isDefined(x) {
  var undefined;
  return x !== undefined;
}

const predefinedMappings = {
  // TODO: is this the right "magic string" for the original dotsies mapping?
  // cribbed from https://github.com/refola/quikdot-sans/blob/5454a3b8c712d1424f5c189c7cf047d1a1310cc3/generate_dotsies.sh#L97
  "-ediclhvbnkugts-aomyjxr-fwqzp---": "default",
  "-kjyxwpiqufnbsh-zcmtgar-vodel---": "densest",
  "-iorachktmlvsbp-ewuqdxy-njgzf---": "sparsest"
};

const buttons = Array.from(document.getElementsByName("mapping_type"));
const customButton = buttons.find(b => b.value === "custom");

const predefinedButtons = {};
for (const mapping in predefinedMappings) {
  predefinedButtons[mapping] = buttons.find((b) => {
    return b.value === predefinedMappings[mapping];
  })
}

const caps = document.getElementById("caps");
const multiple = document.getElementById("multiple");
const currentMapping = document.getElementById("mapping");

const options = {};

chrome.storage.sync.get("options", (data) => {
  Object.assign(options, data.options);
  if (isDefined(options.caps)) {
    caps.checked = Boolean(options.caps);
  }
  if (isDefined(options.multiple)) {
    multiple.checked = Boolean(options.multiple);
  }
  if (isDefined(options.mapping)) {
    currentMapping.value = options.mapping;
    (predefinedButtons[mapping.value] || customButton).checked = true;
  }
});

caps.addEventListener("change", (event) => {
  options.caps = event.target.checked;
  chrome.storage.sync.set({options});
});

multiple.addEventListener("change", (event) => {
  options.multiple = event.target.checked;
  chrome.storage.sync.set({options});
});

const AUTOSAVE_MS = 750;
let saveTimer = null;

function saveOptions() {
  clearTimeout(saveTimer);
  saveTimer = null;
  chrome.storage.sync.set({options});
}

currentMapping.addEventListener("change", (event) => {
  options.mapping = event.target.value;
  saveOptions();
});

currentMapping.addEventListener("input", (event) => {
  if (!event.isComposing) {
    (predefinedButtons[event.target.value] || customButton).checked = true;
    options.mapping = event.target.value;
  }
  if (!saveTimer) {
    saveTimer = setTimeout(saveOptions, AUTOSAVE_MS, event.target.value);
  }
});

for (const mapping in predefinedButtons) {
  predefinedButtons[mapping].addEventListener("change", (event) => {
    if (event.target.checked) {
      currentMapping.value = mapping;

      currentMapping.dispatchEvent(new Event("change"));
    }
  });
}

const shuffle = document.getElementById("shuffle");
shuffle.addEventListener("click", (event) => {
  let shuf = currentMapping.value.replace(/-/g, '').split("");
  for (let i = shuf.length - 1; i > 0; i--) {
      const j = Math.floor(Math.random() * (i + 1));
      [shuf[i], shuf[j]] = [shuf[j], shuf[i]];
  }

  let re = /[^-]+/g;
  let i = 0;
  while (i < shuf.length) {
    let match = re.exec(currentMapping.value);
    currentMapping.setRangeText(
        shuf.slice(i, (i += match[0].length)).join(""),
        match.index, match.index + match[0].length,
    );
  }

  currentMapping.dispatchEvent(new Event("input"));
  currentMapping.dispatchEvent(new Event("change"));
});
