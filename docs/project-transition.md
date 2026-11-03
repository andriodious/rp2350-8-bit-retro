# Publishing the new project

The prepared source is local; these instructions perform the GitHub transition.
The recommended route is to rename your existing fork, retaining its existing
branches, history and upstream relationship. The prepared `retro-platform` branch
includes the C64 improvements, tested Nimbus implementation and new project docs.
The old PR branch remains available; no force push is required.

## 1. Rename your fork

On **andriodious/rp2350-bbc-micro**, open Settings, change Repository name to
`rp2350-8-bit-retro`, then Rename. Make sure this is your fork, not 8bitkick's repo.
GitHub documents this procedure and its redirects here:
https://docs.github.com/en/repositories/creating-and-managing-repositories/renaming-a-repository

Leave `andriodious/frank-c64` in place: builds still fetch its pinned revision.

## 2. Import the prepared branch with its history

Copy `rp2350-8-bit-retro.bundle` from the handoff package to your Pi, then run
these commands from the directory containing that file. Use a new destination
folder; keep the old `rp2350-c64-pr` working copy intact.

```sh
git clone -b retro-platform rp2350-8-bit-retro.bundle rp2350-8-bit-retro
cd rp2350-8-bit-retro
git remote set-url origin git@github.com:andriodious/rp2350-8-bit-retro.git
git remote add upstream https://github.com/8bitkick/rp2350-bbc-micro.git
git status --short
git log -1 --oneline
git push -u origin retro-platform
```

Use HTTPS instead if that is your working authentication method. The bundle has
Git history; the source ZIP is for inspection/building and does not contain `.git`.
The completed branch can be reviewed before the push. Its history retains the
original upstream and the successive local integration commits, rather than
replacing everything with a source-only initial commit.

## 3. Make the platform branch the default

In your renamed repository's Settings, change Default branch to `retro-platform`.
Keep `main` and `presto-c64-r11` as historical branches for now. See:
https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-branches-in-your-repository/changing-the-default-branch

If Actions is disabled on the fork, enable it in GitHub and run the build workflow.
The workflow builds individual machine targets and the branded combined image.
No successful GitHub CI result is claimed before these workflows actually run.

Suggested About description:

> BBC Model B, BBC Master and Commodore 64 emulation on Pimoroni Presto, with web/SD interfaces and Bluetooth controller support.

## 4. Build, test and publish a release

Follow `docs/build-retro.md`, test the branded firmware on the Presto, then use
`docs/releases/retro-initial.md` as release notes. The handoff's built UF2 is also
available for testing. Publish the corresponding source alongside any firmware,
retaining component licences and asset notices. Review the ROM/game notices for
any embedded assets you distribute; this project does not grant rights to them.

The maintainer offered to link the new project. Once its URL and README are live,
you can send the link yourself. Nothing in this preparation messages the maintainer,
renames a remote repository, pushes a branch or publishes a GitHub release.
