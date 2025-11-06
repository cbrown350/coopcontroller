const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

// Directory to search files in
const rootDir = path.resolve(__dirname, '../../data');

// File extensions to match
const extensions = ['.htm', '.css', '.js', '.ico'];

function gzipFile(filepath) {
  const gzip = zlib.createGzip();
  const input = fs.createReadStream(filepath);
  const output = fs.createWriteStream(`${filepath}.gz`);

  input.pipe(gzip).pipe(output);
  output.on('finish', () => {
    console.log(`Gzipped: ${filepath}`);
  });
  output.on('error', (err) => {
    console.error(`Error gzipping ${filepath}:`, err);
  });
}

function walkDir(dir) {
  fs.readdir(dir, { withFileTypes: true }, (err, entries) => {
    if (err) {
      console.error(`Error reading directory ${dir}:`, err);
      return;
    }
    entries.forEach((entry) => {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        walkDir(fullPath);
      } else {
        const ext = path.extname(entry.name).toLowerCase();
        if (extensions.includes(ext)) {
          gzipFile(fullPath);
        }
      }
    });
  });
}

// Start walking from rootDir
walkDir(rootDir);
