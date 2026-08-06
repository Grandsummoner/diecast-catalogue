# 🏎️ APEX ARCHIVE — Diecast Collector Index & Poster Studio

An editorial, local-first photo catalog and poster generator built specifically for diecast hobby collectors (Hot Wheels, Matchbox, Mini GT, Kaido House, AutoArt, Tarmac Works, Tomica, and more).

Designed with a **Swiss Archival Aesthetic**: light theme only, oversized display typography, high-contrast crisp borders, and zero generic "AI slop" gradients.

---

## ✨ Features

- 📸 **High-Volume Cataloging**: Uses client-side IndexedDB to store and index hundreds to thousands of high-res diecast photos without crashing browser memory.
- 🤖 **AI Vision & 1:1 History Pipeline**: Analyzes uploaded photos to identify the model/casting and attaches a 2-sentence real-world automotive/motorsport history nugget.
- 🎨 **Poster Studio**: Transforms any cataloged diecast model into a high-resolution, exportable PNG graphic card / spec sheet for Instagram or collector forums.
- 🔍 **Search & Faceted Filters**: Fast client-side filtering by scale (`1:64`, `1:43`, `1:18`), brand, car make, and custom search queries.
- 🔒 **100% Private & Local**: Photos are stored directly inside your browser. No database servers or monthly hosting fees required.
- ⚡ **Zero Setup Required**: Runs as a single static file directly via GitHub Pages.

---

## 🚀 Live Demo / Deployment

### How to Host on GitHub Pages (Free)

1. Fork or upload `index.html` to your GitHub repository.
2. Go to **Settings** -> **Pages**.
3. Under **Branch**, select `main` and `/ (root)`.
4. Click **Save**.
5. Your catalog is now live at `https://<your-username>.github.io/<your-repo-name>/`.

---

## 🔑 AI Identification & 1:1 History Setup (Optional)

Apex Archive works out-of-the-box in **Mock Mode** (no API key required).

To enable real-time AI recognition and automatic 1:1 car history retrieval:
1. Get a free API key from [Google AI Studio](https://aistudio.google.com/).
2. Click the **🔑 API Key** button in the header of the app.
3. Paste your key. It is stored safely in your browser's `localStorage`.

---

## 🛠️ Built With

- **Styling**: Tailwind CSS (CDN)
- **Icons**: Lucide Icons
- **Database**: HTML5 IndexedDB API
- **Export Engine**: `html2canvas`
- **AI Engine**: Google Gemini 2.5 Flash Vision API

---

## 📄 License

MIT License — Feel free to customize and adapt for your own collection!
