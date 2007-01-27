
import unittest
import plash.pola_run_args as pola_run_args


class TestFlags(unittest.TestCase):

    def test(self):
        self.assertEqual(pola_run_args.split_flags(""),
                         [])
        self.assertEqual(pola_run_args.split_flags("al,objrw,foo"),
                         ["a", "l", "objrw", "foo"])


if __name__ == '__main__':
    unittest.main()
