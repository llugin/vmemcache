#!/usr/bin/env python3

import matplotlib.pyplot as plt
import argparse

MARKERS = ('o', '^', 's', 'D', 'X')


def _add_plot(series, label, marker):
    plt.plot(series, label=label, marker=marker, linestyle=':', linewidth=0.5,
             markersize=4)


def _parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('out', nargs='*', help='Create a plot for all provided'
                        ' output files')
    args = parser.parse_args()
    if not args.out:
        parser.error('at least one output need to be provided')
    return args


def _draw_plot():
    plt.yscale('log')
    plt.xticks(list(range(0, 101, 5)))
    plt.xlabel('latency [%]')
    plt.grid(True)
    plt.ylabel('operation time [ns]')
    plt.legend()
    plt.show()


def _read_out(path):
    with open(path, 'r') as f:
        out = f.readlines()
        hits = [float(h) for h in out[0].split(';')]
        misses = [float(m) for m in out[1].split(';')]
        return hits, misses


def draw_plots(outputs, hits=True, misses=True):
    for i, out in enumerate(outputs):
        hits, misses = _read_out(out)
        if hits:
            _add_plot(hits, '{}_hits'.format(out), MARKERS[i % len(MARKERS)])
        if misses:
            _add_plot(misses, '{}_misses'.format(out),
                      MARKERS[i & len(MARKERS)])
    _draw_plot()


def _main():
    args = _parse_args()
    draw_plots(args.out)


if __name__ == '__main__':
    _main()
