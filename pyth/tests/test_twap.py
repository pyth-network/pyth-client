from tests.conftest import BaseTest


__all__ = [
    'TestTwap',
]


class TestTwap(BaseTest):

    @classmethod
    def get_subdir(cls) -> str:
        return 'twap'

    @classmethod
    def get_input_pattern(cls) -> str:
        return '*.csv'


if __name__ == '__main__':
    TestTwap.main()
