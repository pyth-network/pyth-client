from __future__ import absolute_import

import os
from glob import glob
from subprocess import check_output

_this_path = os.path.abspath(__file__)
_this_dir = os.path.dirname(_this_path)


def _get_test_class():

  test_folder = os.path.join(_this_dir, 'qset')

  class TestQset(object):

    def run_test(self, test_id):
      test_file = os.path.join(test_folder, '%s.json' % (test_id,))
      output = check_output(['test_qset', test_file])
      output = output.decode()
      reg_file = os.path.join(test_folder, '%s.result' % (test_id,))
      with open(reg_file, 'r') as f:
        reg_output = f.read()
      assert output == reg_output

  test_files = os.path.join(test_folder, '*.json')
  for test_file in glob(test_files):
    test_id = os.path.basename(test_file).rsplit('.')[0]

    def test_func(self):
      self.run_test(test_id)
    setattr(TestQset, 'test_%s' % (test_id,), test_func)

  return TestQset


TestQset = _get_test_class()
