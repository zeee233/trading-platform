xml_content = '''<?xml version="1.0" encoding="UTF-8"?>
<create>
 <account id="123456" balance="1000"/>
 </symbol>
</create>
'''

with open("sample2.xml", "w") as xml_file:
    xml_file.write(xml_content)
print("Success!")