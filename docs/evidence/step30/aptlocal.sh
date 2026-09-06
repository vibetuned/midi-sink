# Local end-to-end test of the apt repository shape publish-apt.yml builds:
# throwaway key, the deb from build/, the same apt-ftparchive/gpg commands,
# served over plain http to a clean container that installs from it.
set -euo pipefail
S="$1"; DEB="$(ls "$PWD"/build/midi-sink_*_amd64.deb)"
rm -rf "$S/aptlocal"; mkdir -p "$S/aptlocal/gnupg" "$S/aptlocal/www/apt/pool/stable"; chmod 700 "$S/aptlocal/gnupg"
export GNUPGHOME="$S/aptlocal/gnupg"
gpg --batch --passphrase '' --quick-gen-key "midi-sink apt TEST key <test@example.invalid>" ed25519 sign 1d >/dev/null 2>&1
cp "$DEB" "$S/aptlocal/www/apt/pool/stable/"
cd "$S/aptlocal/www/apt"
suite=stable
mkdir -p "dists/$suite/main/binary-amd64"
apt-ftparchive --arch amd64 packages "pool/$suite" > "dists/$suite/main/binary-amd64/Packages"
gzip -kf "dists/$suite/main/binary-amd64/Packages"
apt-ftparchive -o APT::FTPArchive::Release::Origin=midi-sink -o APT::FTPArchive::Release::Label=midi-sink \
  -o APT::FTPArchive::Release::Suite="$suite" -o APT::FTPArchive::Release::Codename="$suite" \
  -o APT::FTPArchive::Release::Components=main -o APT::FTPArchive::Release::Architectures=amd64 \
  release "dists/$suite" > "dists/$suite/Release"
gpg --batch --yes --armor --detach-sign -o "dists/$suite/Release.gpg" "dists/$suite/Release"
gpg --batch --yes --clearsign -o "dists/$suite/InRelease" "dists/$suite/Release"
gpg --batch --armor --export > midi-sink.asc
echo "--- repository tree ---"; find . -type f | sort
cd "$S/aptlocal/www"; python3 -m http.server 8765 --bind 0.0.0.0 >/dev/null 2>&1 & SRV=$!; sleep 1
echo "--- clean ubuntu:24.04: add the repo exactly as the docs say (host = 172.17.0.1), install, run ---"
docker run --rm --add-host=host.docker.internal:host-gateway ubuntu:24.04 bash -euc '
  apt-get update -qq >/dev/null; DEBIAN_FRONTEND=noninteractive apt-get install -y -qq curl ca-certificates >/dev/null
  install -d -m 0755 /etc/apt/keyrings
  curl -fsS http://host.docker.internal:8765/apt/midi-sink.asc -o /etc/apt/keyrings/midi-sink.asc
  echo "deb [signed-by=/etc/apt/keyrings/midi-sink.asc] http://host.docker.internal:8765/apt stable main" > /etc/apt/sources.list.d/midi-sink.list
  apt-get update -qq 2>&1 | grep -iE "error|warn|NO_PUBKEY" || true
  apt-cache policy midi-sink | head -6
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq midi-sink >/dev/null
  echo "installed from the repo: $(dpkg-query -W -f "\${Version}" midi-sink)"
  midi-sink --version
  test -f /usr/share/applications/midi-sink.desktop && echo "desktop entry present"' 2>&1 | tail -12
kill $SRV
