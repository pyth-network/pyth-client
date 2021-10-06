import argparse
import glob
import inspect
import os
import subprocess


__all__ = [
    'BaseTest',
]


class BaseTest:

    def __init_subclass__(cls, **kwargs):
        super().__init_subclass__(**kwargs)
        funcs = cls.gen_test_funcs()
        for func in funcs:
            setattr(cls, func.__name__, func)

    @classmethod
    def gen_test_funcs(cls):
        paths = cls.get_input_paths()
        return [cls.get_test_method(path) for path in paths]

    @classmethod
    def get_subdir(cls) -> str:
        raise NotImplementedError()

    @classmethod
    def get_absdir(cls) -> str:
        path = os.path.abspath(inspect.getfile(cls))
        return os.path.join(os.path.dirname(path), cls.get_subdir())

    @classmethod
    def get_input_pattern(cls) -> str:
        raise NotImplementedError()

    @classmethod
    def get_input_paths(cls) -> list[str]:
        pattern = os.path.join(cls.get_absdir(), cls.get_input_pattern())
        return glob.glob(pattern)

    @classmethod
    def get_output_path(cls, input_path: str) -> str:
        base, _ = os.path.splitext(input_path)
        return base + '.result'

    @classmethod
    def get_test_id(cls, path: str) -> str:
        test_id, _ = os.path.splitext(os.path.basename(path))
        return test_id

    @classmethod
    def get_test_method(cls, input_path: str):

        def test_func(self: BaseTest, *, inplace: bool = False):
            output_path = self.get_output_path(input_path)
            self.run_test(input_path, output_path, inplace=inplace)

        test_id = cls.get_test_id(input_path)
        test_func.__name__ = 'test_' + test_id
        return test_func

    @classmethod
    def get_arg_parser(cls):
        parser = argparse.ArgumentParser()
        parser.add_argument('--inplace', action='store_true')
        return parser

    @classmethod
    def main(cls):
        parser = cls.get_arg_parser()
        args = parser.parse_args()
        inst = cls()
        funcs = inst.gen_test_funcs()
        for func in funcs:
            func(inst, inplace=args.inplace)

    def get_subprocess_args(self, input_path: str) -> list[str]:
        cmd = 'test_' + self.get_subdir()
        return [cmd, input_path]

    def gen_output(self, input_path: str) -> str:
        args = self.get_subprocess_args(input_path)
        output = subprocess.check_output(args)
        return output.decode()

    def run_test(self, input_path: str, output_path: str, *, inplace: bool):
        with open(output_path, 'r') as f:
            expected = f.read()
        actual = self.gen_output(input_path)

        if inplace:
            if actual != expected:
                with open(output_path, 'w') as f:
                    f.write(actual)
        else:
            assert actual == expected
