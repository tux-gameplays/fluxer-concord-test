#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
catalog=docs/FLUXER_COMPATIBILITY.md

if [ ! -f "$catalog" ]; then
    echo "Missing compatibility catalog: $catalog" >&2
    exit 1
fi

symbols=$(
    for header in include/*.h; do
        case "$header" in
            include/discord-internal.h|include/discord-request.h|\
            include/discord-response.h|include/discord-worker.h)
                continue
                ;;
        esac
        grep -hE '^[[:space:]]*(CCORDcode|void|bool|int|unsigned|size_t|char|const|struct|enum|u64snowflake|uint64_t|uint32_t|long)[[:space:]].*discord_[[:alnum:]_]+[[:space:]]*\(' "$header" || true
    done |
        grep -oE 'discord_[[:alnum:]_]+[[:space:]]*\(' |
        sed -E 's/[[:space:]]*\($//' |
        sort -u
)

if [ -z "$symbols" ]; then
    echo "No public Concord functions found" >&2
    exit 1
fi

failures=0
for symbol in $symbols; do
    count=$(grep -F -c '| `'"$symbol"'` |' "$catalog" || true)
    if [ "$count" -ne 1 ]; then
        echo "$symbol: expected exactly one catalog row, got $count" >&2
        failures=1
        continue
    fi

    status=$(grep -F '| `'"$symbol"'` |' "$catalog" |
        sed -E 's/^\| `[^`]+` \| [^|]+ \| ([^|]+) \|.*/\1/' |
        tr -d ' ')
    case "$status" in
        supported|adapted|unsupported|pending) ;;
        *)
            echo "$symbol: invalid catalog status '$status'" >&2
            failures=1
            ;;
    esac
    case "$symbol" in
        *application_command*|*interaction*)
            if [ "$status" != unsupported ]; then
                echo "$symbol: Fluxer has no equivalent command/interaction API; expected unsupported" >&2
                failures=1
            fi
            ;;
    esac
    if [ "${1:-}" = "--complete" ] && [ "$status" = pending ]; then
        echo "$symbol: still pending" >&2
        failures=1
    fi
done

catalog_symbols=$(sed -nE 's/^\| `(discord_[^`]+)` \|.*/\1/p' "$catalog")
for symbol in $catalog_symbols; do
    if ! printf '%s\n' "$symbols" | grep -Fxq "$symbol"; then
        echo "$symbol: catalog row does not match a public declaration" >&2
        failures=1
    fi
done

if [ "$failures" -ne 0 ]; then
    exit 1
fi
echo "Compatibility catalog covers $(printf '%s\n' "$symbols" | wc -l) public functions."
