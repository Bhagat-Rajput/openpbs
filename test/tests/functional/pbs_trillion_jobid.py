# coding: utf-8

# Copyright (C) 1994-2018 Altair Engineering, Inc.
# For more information, contact Altair at www.altair.com.
#
# This file is part of the PBS Professional ("PBS Pro") software.
#
# Open Source License Information:
#
# PBS Pro is free software. You can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# PBS Pro is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.
# See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Commercial License Information:
#
# For a copy of the commercial license terms and conditions,
# go to: (http://www.pbspro.com/UserArea/agreement.html)
# or contact the Altair Legal Department.
#
# Altair’s dual-license business model allows companies, individuals, and
# organizations to create proprietary derivative works of PBS Pro and
# distribute them - whether embedded or bundled with other software -
# under a commercial license agreement.
#
# Use of Altair’s trademarks, including but not limited to "PBS™",
# "PBS Professional®", and "PBS Pro™" and Altair’s logos is subject to Altair's
# trademark licensing policies.

from tests.functional import *


class TestTrillionJobid(TestFunctional):
    """
    This test suite tests the Trillion Job ID
    """

    def test_set_unset_max_job_sequence_id(self):
        """
        Set/Unset max_job_sequence_id
        """
        # Non-admin user
        seq_id = {ATTR_max_job_sequence_id: 123456789}
        try:
            self.server.manager(MGR_CMD_SET, SERVER, seq_id, runas=TEST_USER1)
        except PbsManagerError as e:
        	self.assertTrue('Unauthorized Request' in e.msg[0])
        try:
            self.server.manager(MGR_CMD_UNSET, SERVER, 'max_job_sequence_id', runas=TEST_USER1)
        except PbsManagerError as e:
            self.assertTrue('Unauthorized Request' in e.msg[0])

        # Admin User
        self.server.manager(MGR_CMD_SET, SERVER, seq_id, runas=ROOT_USER, expect=True)
        self.server.expect(SERVER, seq_id)
        self.server.log_match("svr_max_job_sequence_id set to val %d" % (seq_id[ATTR_max_job_sequence_id]),
                              starttime=self.server.ctime)
        self.server.manager(MGR_CMD_UNSET, SERVER, 'max_job_sequence_id', runas=ROOT_USER, expect=True)
        self.server.log_match("svr_max_job_sequence_id reverting back to default val %d" % (9999999),
                              starttime=self.server.ctime)

    def test_max_job_sequence_id_values(self):
    	"""
        Test to check valid/invalid values for the max_job_sequence_id attribute
    	"""
    	# Invalid Values
    	invalid_values = [-9999999, '*456879846', 23545.45, 'ajndd', '**45', 'asgh456']
    	for val in invalid_values:
    		try:
    			seq_id  = {ATTR_max_job_sequence_id: val}
    			self.server.manager(MGR_CMD_SET, SERVER, seq_id, runas=ROOT_USER)
    		except PbsManagerError as e:
    			self.assertTrue('Illegal attribute or resource value' in e.msg[0])
        # Less than or Greater than the attribute limit
    	min_max_values = [120515, 999999, 1234567891234, 9999999999999]
    	for val in min_max_values:
    		try:
    			seq_id  = {ATTR_max_job_sequence_id: val}
    			self.server.manager(MGR_CMD_SET, SERVER, seq_id, runas=ROOT_USER)
    		except PbsManagerError as e:
    			self.assertTrue('Cannot set max_job_sequence_id < 9999999, or > 999999999999' in e.msg[0])
    	# Valid values
    	valid_values = [9999999, 123456789, 100000000000, 999999999999]
    	for val in valid_values:
    		seq_id  = {ATTR_max_job_sequence_id: val}
    		self.server.manager(MGR_CMD_SET, SERVER, seq_id, runas=ROOT_USER, expect=True)
        	self.server.expect(SERVER, seq_id)


