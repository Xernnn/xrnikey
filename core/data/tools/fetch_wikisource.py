"""Fetch plain text of public-domain works from vi.wikisource (Wikisource only hosts PD texts).

usage: fetch_wikisource.py OUTDIR TITLE_PREFIX...  (writes OUTDIR/ws.txt and ws-titles.txt)
"""
import json, sys, time, urllib.parse, urllib.request, urllib.error, re, html
API = "https://vi.wikisource.org/w/api.php"
UA = {"User-Agent": "xk-data-check/0.1 (generic client)"}
def get(params):
    params["format"] = "json"
    url = API + "?" + urllib.parse.urlencode(params)
    for attempt in range(6):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
                return json.load(r)
        except urllib.error.HTTPError as e:
            if e.code != 429:
                raise
            time.sleep(10 * (attempt + 1))
    raise RuntimeError("gave up")
out_dir = sys.argv[1]
prefixes = sys.argv[2:]
titles = []
for p in prefixes:
    d = get({"action": "query", "list": "allpages", "apprefix": p, "aplimit": "500"})
    titles += [x["title"] for x in d["query"]["allpages"]]
print(len(titles), "pages", flush=True)
with open(out_dir + "/ws.txt", "w", encoding="utf-8") as f, open(out_dir + "/ws-titles.txt", "w", encoding="utf-8") as ft:
    for t in titles:
        try:
            d = get({"action": "parse", "page": t, "prop": "text", "redirects": "1"})
        except Exception as e:
            print("fail", t, e, flush=True); continue
        h = d.get("parse", {}).get("text", {}).get("*", "")
        txt = html.unescape(re.sub(r"<[^>]+>", " ", re.sub(r"(?s)<(style|script)[^>]*>.*?</\1>", " ", h)))
        f.write(txt + "\n")
        ft.write(t + "\n")
        time.sleep(2)
print("done", flush=True)
