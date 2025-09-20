import re, requests, ast

url = "https://raw.githubusercontent.com/openframeworks/openFrameworks/58e5d68da6afd1235865665613b3582a3e713067/addons/ofxKinect/src/extra/ofxKinectExtras.cpp"
src = requests.get(url).text

for name in ("fwk4wBin", "fw1473Bin"):
    m = re.search(rf"{name}\[\]\s*=\s*\{{([^}}]+)\}};", src, re.S)
    if not m:
        continue
    array_text = m.group(1)
    # Clean and split into list of ints
    data = [int(x, 16) for x in array_text.replace("\n","").split(",") if x.strip()]
    with open(f"{name.replace('Bin','')}.bin", "wb") as f:
        f.write(bytes(data))
