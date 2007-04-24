
import unittest

import plash.mainloop


class ReasonsToRunTest(unittest.TestCase):

    def test_dispose_reason(self):
        count = plash.mainloop._reasons_count
        reason = plash.mainloop.ReasonToRun()
        self.assertEquals(plash.mainloop._reasons_count, count + 1)
        reason.dispose()
        self.assertEquals(plash.mainloop._reasons_count, count)

    def test_drop_reason(self):
        # Check that GCing the reason disposes it
        count = plash.mainloop._reasons_count
        plash.mainloop.ReasonToRun()
        self.assertEquals(plash.mainloop._reasons_count, count)


if __name__ == "__main__":
    unittest.main()
