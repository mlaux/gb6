let total = 0;
for (let row = 1; row < rows.length; row++) {
	let cols = rows[row].getElementsByTagName("td");
    for (let col = 1; col < cols.length; col++) {
		let html = cols[col].innerHTML;
		if (html.indexOf("<br>") != -1) {
			html = html.substring(0, html.indexOf("<br>"));
        }
		console.log("{ 0x" + total.toString(16) + ", \"" + html + "\" },");
		total++;
    }
}

