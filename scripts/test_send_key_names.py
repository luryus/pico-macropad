import hid
from itertools import chain

d = hid.device()
d.open(vendor_id=0x2e8a, product_id=0xffee)
print(d.get_manufacturer_string())

r = [0x04] + list(chain.from_iterable([[i]*4 for i in range(56, 68)]))

assert len(r) == 49

d.send_feature_report(r)