import os
import argparse
import subprocess

if __name__ == "__main__":
  argparser = argparse.ArgumentParser()
  argparser.add_argument(
    "-t",
    "--target_list",
    type=str,
    help="File that has the list of target function, loop pairs",
    default="target_list"
  )
  argparser.add_argument(
    "-b",
    "--benchmark",
    help="Path to bechmark",
    default="/scratch/yc0769/parallelizer-workspace/gino/tests/regression/workspace"
  )

  args = argparser.parse_args()
  if not os.path.exists(args.benchmark):
    raise RuntimeError(f"{args.benchmark} does not exist")

  os.chdir(args.benchmark)

  if not os.path.exists(args.target_list):
    raise RuntimeError(f"{args.target_list} does not exist")

  subprocess.run(['rm', 'result.slamp.profile'])

  target_list = []
  with open(args.target_list, "r") as targetfile:
    for line in targetfile:
      target_pair = {'func' : line.split()[0], 'loop' : line.split()[1]}
      target_list.append(target_pair)

  print(target_list)

  for target in target_list:
    subprocess.run(['make', 'result.slamp.profile'], env=dict(os.environ, TARGETFCN=target['func'], TARGETLOOP=target['loop']))
