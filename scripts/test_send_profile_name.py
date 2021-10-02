import hid

d = hid.device()
d.open(vendor_id=0x2e8a, product_id=0xffee)
print(d.get_manufacturer_string())

r = [0x03] + list(range(65, 73))

d.send_feature_report(r)