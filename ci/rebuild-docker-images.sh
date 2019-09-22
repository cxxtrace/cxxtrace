#!/bin/sh
set -eu
cd "$(dirname "${0}")"

publish=0
while [ "${#}" -ne 0 ]; do
  case "${1}" in
    --publish)
      publish=1
      shift
      ;;
    *)
      printf 'error: unknown option: %s\n' "${1}" >&2
      exit 1
      ;;
  esac
done

set -x
docker build --tag=strager/cxxtrace-ubuntu-19.04-gcc-8 - <cxxtrace-ubuntu-19.04-gcc-8.Dockerfile
if [ "${publish}" -eq 1 ]; then
  docker push strager/cxxtrace-ubuntu-19.04-gcc-8
fi
