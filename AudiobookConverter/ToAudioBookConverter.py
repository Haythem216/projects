from gtts import gTTS
from PyPDF2 import PdfReader

reader = PdfReader("tester.pdf")

text_parts = []
for page in reader.pages:
    t = page.extract_text()
    if t:
        text_parts.append(t)

full_text = "\n".join(text_parts)

tts = gTTS(full_text, lang="en")
tts.save("audiobook.mp3")

print("Saved audiobook.mp3")




"""from pyttsx3 import*
from PyPDF2 import*


def speak_text(engine, text, chunk_size=500):
    for i in range(0, len(text), chunk_size):
        chunk = text[i:i+chunk_size]
        engine.say(chunk)
        engine.runAndWait()





engine = init()

file_path=str(input("enter your pdf path: "))
print("\nParsing...")

reader=PdfReader(file_path)
page_num=len(reader.pages)
page_length= "I detected "+str(page_num)+" pages."
engine.say(page_length)
engine.runAndWait()

for i in range(len(reader.pages)):
    print(f"Reading page {i+1}")
    text = reader.pages[i].extract_text()
    if text:
        speak_text(engine,text)"""
