let total = 0;
let rows = document.getElementsByTagName('tr');
let val = "";
for (let row = 1; row < rows.length; row++) {
  let cols = rows[row].getElementsByTagName("td");
  for (let col = 0; col < cols.length; col++) {
    let html = cols[col].innerText.split('\n');
    if (html.length >= 2) {
      let insnName = html[0];
      let cycleCounts = html[1].split(' ')[1];
      let branchParts = cycleCounts.split('-');

      let cycleCount = branchParts[0].substring(0, branchParts[0].length - 1);
      let cycleCountBranch = cycleCount;
      if (branchParts.length > 1) {
        cycleCountBranch = branchParts[1].substring(0, branchParts[1].length - 1);
      }
      val += "{ 0x" + total.toString(16) + ", \"" + insnName + "\", " + cycleCount + ", " + cycleCountBranch + " },";
    } else {
      val += "{ 0x" + total.toString(16) + ", \"<invalid>\", 0, 0 },";
    }
    total++;
  }
}
console.log(val);