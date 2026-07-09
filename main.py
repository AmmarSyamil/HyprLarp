import argparse
# from gui import run_gui

parser = argparse.ArgumentParser()

parser.add_argument(
    "--gui",
    action="store_true",
    help="Launch the graphical interface"
)

args = parser.parse_args()

if args.gui:
    run_gui()

