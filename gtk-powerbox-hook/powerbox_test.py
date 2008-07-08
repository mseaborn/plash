
import os
import unittest

import plash.env
import plash.filedesc
import plash.mainloop
import plash.marshal
import plash.process


class PowerboxRequester(plash.marshal.Pyobj_demarshal):

    def __init__(self):
        self.calls = []

    # TODO: this explicit marshalling should not be necessary
    def cap_call(self, args):
        marshaller = plash.marshal.methods_by_name["powerbox_req_filename"]
        result = self.powerbox_req_filename(
            marshaller["packer"].unpack_r(args))
        return plash.marshal.pack("powerbox_result_filename", result)

    def powerbox_req_filename(self, args):
        self.calls.append(args)
        return "/home/foo/example-pathname"


class CancellingPowerboxRequester(PowerboxRequester):

    def powerbox_req_filename(self, args):
        self.calls.append(args)
        raise Exception("request cancelled")


class PowerboxTest(unittest.TestCase):

    def _test_powerbox(self, test_name, pb_requester):
        proc = plash.process.ProcessSpecWithNamespace()
        proc.caps["powerbox_req_filename"] = pb_requester
        proc.cmd = "./test-gtk-powerbox"
        proc.args = [test_name]
        proc.env = os.environ.copy()
        proc.get_namespace().attach_at_path("/", plash.env.get_root_dir())
        proc.cwd_path = os.getcwd()
        for fd in (0, 1, 2):
            proc.fds[fd] = plash.filedesc.dup_and_wrap_fd(fd)
        pid = proc.spawn()
        plash.mainloop.run_server()
        pid2, status = os.waitpid(pid, 0)
        self.assertEquals(status, 0)

    def test_open_file(self):
        pb_requester = PowerboxRequester()
        self._test_powerbox("test_open_file", pb_requester)
        self.assertEquals(pb_requester.calls, [[]])

    def test_save_file(self):
        pb_requester = PowerboxRequester()
        self._test_powerbox("test_save_file", pb_requester)
        self.assertEquals(pb_requester.calls, [[["Save"]]])

    def test_cancellation(self):
        pb_requester = CancellingPowerboxRequester()
        self._test_powerbox("test_cancellation", pb_requester)
        self.assertEquals(pb_requester.calls, [[]])

    def test_parent_window_id(self):
        pb_requester = PowerboxRequester()
        self._test_powerbox("test_parent_window_id", pb_requester)
        self.assertEquals(len(pb_requester.calls), 1)
        self.assertEquals(len(pb_requester.calls[0]), 2)
        self.assertEquals(pb_requester.calls[0][0], ["Save"])
        self.assertEquals(pb_requester.calls[0][1][0], "Transientfor")

    # TODO: make this test pass
#     def test_multiple_requests(self):
#         pb_requester = PowerboxRequester()
#         self._test_powerbox("test_multiple_requests", pb_requester)
#         self.assertEquals(pb_requester.calls, [[], []])

    def test_success_response_code(self):
        self._test_powerbox("test_success_response_code", PowerboxRequester())


if __name__ == "__main__":
    unittest.main()
