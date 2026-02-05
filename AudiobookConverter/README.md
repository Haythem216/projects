# PDF → Audiobook Converter

**Status: In progress**

Python script that converts PDF text to speech and saves it as an MP3. Uses gTTS and PyPDF2. Still being improved (e.g. configurable input path, more options).

## Usage

```bash
pip install gTTS PyPDF2
# Place your PDF as tester.pdf in this directory, or edit the script to use your path.
python ToAudioBookConverter.py
```

Output: `audiobook.mp3` in the current directory.

## Tech

- Python 3, gTTS, PyPDF2
