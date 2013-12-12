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
		if not sep or remote == "localhost":
			if remote != "localhost":
				dir_path = remote
				remote = ""
			host = Host()
		else:
			host = RemoteHost()
		host.__dict__.update(
			dir_path=dir_path, remote=remote, sha1_path=sha1_path
		)
		return host

	@property
	def unison_clean_cmd(self):
		return [
			"bash", "-c",
			("find ~/.unison/ | { egrep /..[0-9a-f]{32}$ || true; }"
			 " | xargs rm -f")
		]

	@property
	def unison_root(self):
		return self.dir_path

	@property
	def mkdir_cmd(self):
		return ["mkdir", "-p", self.dir_path]

	def get_cp_cmd(self, sha1):
		return ["cp", "-f", sha1_path[sha1], os.path.join(self.dir_path, sha1)]

	def get_rm_cmd(self, sha1):
		return ["rm", "-f", os.path.join(self.dir_path, sha1)]

	def get_ls_cmd(self):
		return ["ls", "-w", "1", self.dir_path]

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
	def unison_clean_cmd(self):
		return self.to_ssh_cmd(super(RemoteHost, self).unison_clean_cmd)

	@property
	def unison_root(self):
		return "rsh://%s/%s" % (
			self.remote,
			super(RemoteHost, self).unison_root
		)

	@property
	def mkdir_cmd(self):
		return self.to_ssh_cmd(super(RemoteHost, self).mkdir_cmd)

	def get_cp_cmd(self, sha1):
		cmd = super(RemoteHost, self).get_cp_cmd(sha1)
		cmd[:-2] = ["scp"]
		cmd[-1] = self.remote + ":" + cmd[-1]
		return cmd

	def get_rm_cmd(self, sha1):
		return self.to_ssh_cmd(super(RemoteHost, self).get_rm_cmd(sha1))

	def get_ls_cmd(self):
		return self.to_ssh_cmd(super(RemoteHost, self).get_ls_cmd)

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
						type=argparse.FileType('r'), default='-')
	po_run_group = parser.add_mutually_exclusive_group()

	po_run_group.add_argument(
		'--unison', help="run unison", action="store_true"
	)
	parser.add_argument(
		'--unison-clean', help="clear unison archives",
		action="store_true"
	)
	parser.add_argument(
		'--unison-cmd', help="path to unison executable",
		default="unison"
	)
	parser.add_argument(
		'--unison-remote-cmd', help="path to unison executable on remote host",
		default='bin/unison'
	)

	po_run_group.add_argument(
		'--jpgsync', help="run jpgsync", action="store_true"
	)
	parser.add_argument(
		'--jpgsync-cmd', help="path to jpgsync executable",
		default="jpgsync"
	)
	parser.add_argument(
		'--jpgsync-remote-cmd',
		help="path to jpgsync executable on remote host",
		default='bin/jpgsync'
	)

	po = parser.parse_args()

	sha1_path = {}
	with open(os.devnull, 'w') as dev_null:
		for path in po.input_file:
			path = path.strip()
			if not path:
				continue
			try:
				output = subprocess.check_output(["jpghash", path])
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
	roots = []
	capture_ip = ""

	with open(os.devnull, 'w') as dev_null:
		def execute_cmd(args, stdout=dev_null, stderr=None):
			print >> sys.stderr, ' '.join(map(repr, args))
			if not po.dry_run:
				subprocess.check_call(args, stdout=stdout, stderr=stderr)

		hosts = []
		s_sha1s1, s_sha1s2 = sample(sha1_path.keys(), po.u, po.s, po.d)

		for dir_path, s_sha1s in zip(
			(po.dir1, po.dir2),
			(s_sha1s1, s_sha1s2),
		):
			host = Host.create(dir_path, sha1_path)
			hosts.append(host)

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

			if po.unison_clean:
				execute_cmd(host.unison_clean_cmd)

			roots.append(host.unison_root)
			capture_ip = capture_ip or host.remote

		for host in hosts:
			execute_cmd(host.get_ls_cmd(), stdout=sys.stderr)

		# compute minimum required traffic (for sanity check later on)
		sym_diff_sha1s = (s_sha1s1 - s_sha1s2) | (s_sha1s2 - s_sha1s1)
		min_traffic = sum(os.path.getsize(sha1_path[s])
						  for s in sym_diff_sha1s)

		if po.dry_run:
			sys.exit(0)

		if po.jpgsync or po.unison:
			cmd = [
				os.path.join(
					os.path.dirname(os.path.realpath(sys.argv[0])),
					"ip-data.sh",
				),
				capture_ip or "localhost",
			]
			if po.unison:
				cmd += [
					po.unison_cmd, "-servercmd", po.unison_remote_cmd,
					"-batch", "-ignorearchives",
					"-terse", "-contactquietly",
				]
				# if no remote, make sure the first one is "rsh://localhost/*"
				if all(map(lambda root: ":" not in root, roots)):
					roots[0] = "rsh://localhost/" + roots[0]
			else:
				cmd += [po.jpgsync_cmd,
						#"-v", "1",
						"-d"]
			cmd += roots
			execute_cmd(cmd, None, dev_null)

			# verify all images are transferred
			all_sha1s = set(s_sha1s1 | s_sha1s2)
			for dir_path, host in zip((po.dir1, po.dir2), hosts):
				assert set(host.get_sha1_basenames()) == all_sha1s
				subprocess.check_call(
					"bash -c \"diff -q <(ls " + dir_path + " | sort)"
					" <(find " + dir_path + " -type f"
					" | xargs jpghash | awk '{print \\$1}' | sort)\"",
					shell=True)

		print min_traffic
