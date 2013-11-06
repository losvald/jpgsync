import argparse
import errno
import os
import random
import subprocess
import sys

def sample(population, union_size, symdiff_size, diff_ratio):
	u = set(random.sample(population, union_size))
	s = set(random.sample(u, symdiff_size))
	i = u - s
	a = set(random.sample(s, int(diff_ratio * symdiff_size))) | i
	b = (s - set(a)) | i
	return a, b

class Host(object):
	@classmethod
	def create(cls, directory, sha1_path):
		remote, sep, dir_path = directory.partition(':')
		if not sep:
			host = Host()
			host.__dict__.update(dir_path=remote, sha1_path=sha1_path)
		else:
			host = RemoteHost()
			host.__dict__.update(dir_path=dir_path, remote=remote,
								 sha1_path=sha1_path)
		return host

	@property
	def mkdir_cmd(self):
		return ["mkdir", "-p", self.dir_path]

	def get_cp_cmd(self, sha1):
		return ["cp", "-f", sha1_path[sha1], os.path.join(dir_path, sha1)]

	def get_rm_cmd(self, sha1):
		return ["rm", "-f", os.path.join(self.dir_path, sha1)]

	def get_sha1_basenames(self):
		"""Generates files' basenames which are valid SHA1 hashes.
		"""
		for basename in os.listdir(self.dir_path):
			if os.path.isfile(os.path.join(self.dir_path, basename)):
				if len(basename) == 40:
					try:
						int(basename, 16)
						yield basename
					except ValueError:
						pass

class RemoteHost(Host):
	def to_ssh_cmd(self, cmd):
		return ["ssh", self.remote, ' '.join(cmd)]

	@property
	def mkdir_cmd(self):
		return self.to_ssh_cmd(super(RemoteHost, self).mkdir_cmd)

	def get_cp_cmd(self, sha1):
		cmd = super(RemoteHost, self).get_cp_cmd(sha1)
		cmd[:-2] = ["scp"]
		return cmd

	def get_rm_cmd(self, sha1):
		return self.to_ssh_cmd(super(RemoteHost, self).get_rm_cmd(sha1))

	def get_sha1_basenames(self):
		"""Generates files' basenames which are valid SHA1 hashes.
		"""
		ls_cmd = r"ls %s | { egrep ^[0-9a-fA-F]{40}$ || true; }" % (
			self.dir_path
		)
		return subprocess.check_output(
			["ssh", self.remote, ls_cmd],
		).strip().split()

if __name__ == '__main__':
	parser = argparse.ArgumentParser()
	parser.add_argument('dir1', metavar="\\[[USER@]HOST]:DIR1")
	parser.add_argument('dir2', metavar="\\[[USER@]HOST]:DIR2")

	po_limit_group = parser.add_mutually_exclusive_group()
	po_limit_group.add_argument(
		'-u', help="bound size of sample union",
		type=int
	)
	po_limit_group.add_argument(
		'-s', help="bound size of sample symmetric difference",
		type=int
	)

	parser.add_argument(
		'-o', help="set overlap ratio",
		type=float, default=.0
	)
	parser.add_argument(
		'-d', help="set sample difference ratio",
		type=float, default=.5
	)
	parser.add_argument('--seed', help="set random seed")
	parser.add_argument(
		'-n', '--dry-run', help="perform a trial run with no changes",
		action="store_true"
	)
	parser.add_argument('-i', '--input-file', help="set input file",
						type=argparse.FileType('r'))
	po = parser.parse_args()

	sha1_path = {}
	with open(os.devnull, 'w') as dev_null:
		for path in sys.stdin:
			path = path.strip()
			if not path:
				continue
			try:
				output = subprocess.check_output(["sha1sum", path])
				sha1_path[output.partition(' ')[0]] = path
			except subprocess.CalledProcessError:
				raise # TODO

	u_max = len(sha1_path)
	if po.s is None:
		if po.u is not None and po.u > u_max:
			parser.exit(1, "Size of sample union too big\n")
		po.u = po.u or u_max
		po.s = int(po.u * (1 - po.o))
	else:
		po.u = po.s + po.o * po.s / (1 - po.o)
		if po.u > u_max:
			parser.exit(1, "Size of sample symmetric difference too big\n")

	random.seed(po.seed)

	with open(os.devnull, 'w') as dev_null:
		def execute_cmd(args):
			print ' '.join(map(repr, args))
			if not po.dry_run:
				subprocess.check_call(args, stdout=dev_null)

		for dir_path, s_sha1s in zip(
			(po.dir1, po.dir2),
			sample(sha1_path.keys(), po.u, po.s, po.d)
		):
			host = Host.create(dir_path, sha1_path)

			# create directory if it doesn't exist
			execute_cmd(host.mkdir_cmd)

			# remove sha1s not present in sample
			dir_sha1s = set(host.get_sha1_basenames())
			for cmd in map(host.get_rm_cmd, dir_sha1s - s_sha1s):
				execute_cmd(cmd)

			# copy sha1s present in sample but not in dir
			for cmd in map(host.get_cp_cmd, s_sha1s - dir_sha1s):
				execute_cmd(cmd)

			# sanity check
			if not po.dry_run:
				assert sorted(host.get_sha1_basenames()) == sorted(s_sha1s)
