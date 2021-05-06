// TODO: should filters.json instead be a .js with "var filterIndex =" at the beginning?
// maybe means we can get rid of the storage requirement and save a permission/fingerprinting possibility
// edit: nah seems to be loading super fast anyway and we need the storage requirement for the filters probably,
// unless we always want to be consuming 66MB of memory whenever the extension is loaded...
(() => {
  const filterIndex = [];

  const filterIndexReq = new XMLHttpRequest();
  filterIndexReq.open("GET", chrome.runtime.getURL("data/filters.json"));
  filterIndexReq.addEventListener("load", (event) => {
    Object.assign(filterIndex, JSON.parse(filterIndexReq.response));

    const percent_cdf = document.createElement("datalist");
    percent_cdf.setAttribute("id", "cdf");

    function addTick(percent) {
      const option = document.createElement("option");
      option.value = percent;
      percent_cdf.appendChild(option);
    }

    addTick(0);
    filterIndex.forEach((filter) => addTick(filter.cdf * 100));
    addTick(100);

    document.body.appendChild(percent_cdf);
  });
  filterIndexReq.send();

  const slider = document.getElementById("slider");

  // chrome.storage.sync.MAX_WRITE_OPERATIONS
  const AUTOSAVE_MS = 60000 / (chrome.storage.sync.MAX_WRITE_OPERATIONS_PER_MINUTE || 120);
  let saveTimer = null;

  function saveSlider() {
    console.log("saving");
    clearTimeout(saveTimer);
    saveTimer = null;
    chrome.storage.sync.set({ "percent": slider.value });
  }
  slider.addEventListener("change", saveSlider);

  const output = document.getElementById("percent");
  slider.addEventListener("input", (event) => {
    let val = event.target.value;
    percent.textContent = Number(val).toFixed(3); //.padStart(7, "0");
    if (!saveTimer) {
        saveTimer = setTimeout(saveSlider, AUTOSAVE_MS, event.target.value);
    }
  });

  chrome.storage.sync.get("percent", (data) => {
    var undefined;
    if (data.percent !== undefined) {
      slider.value = data.percent;
      slider.dispatchEvent(new Event("input"));
    }
  });
})();
