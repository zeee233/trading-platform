import xml.etree.ElementTree as ET

def generate_xml(account_id):
    transactions = ET.Element("transactions")
    transactions.set("id", str(account_id))

    # Add an order child
    order = ET.SubElement(transactions, "order")
    order.set("sym", "SPY")
    order.set("amount", "5000")
    order.set("limit", "10")

    # Add a query child
    """
    query = ET.SubElement(transactions, "query")
    query.set("id", "TRANS_ID")

    # Add a cancel child
    cancel = ET.SubElement(transactions, "cancel")
    cancel.set("id", "TRANS_ID")

    # Convert the XML structure to a string
    """
    xml_string = ET.tostring(transactions, encoding="unicode", method="xml")

    return xml_string

account_id = 123456
xml_content = generate_xml(account_id)

with open("sample2.xml", "w") as xml_file:
    xml_file.write(xml_content)

print("Success!")