from tests.conftest import BaseTest


__all__ = [
    'TestQset',
]


class TestQset(BaseTest):

    @classmethod
    def get_subdir(cls) -> str:
        return 'qset'

    @classmethod
    def get_input_pattern(cls) -> str:
        return '*.json'


if __name__ == '__main__':
    TestQset.main()
