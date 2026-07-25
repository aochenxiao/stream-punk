import type { TokenInfo } from "../parser";
import { CATEGORY_COLORS } from "../parser";
import styles from "./TokenTag.module.css";

interface Props {
  token: TokenInfo;
}

export default function TokenTag({ token }: Props) {
  const color = CATEGORY_COLORS[token.category] ?? "#607d8b";

  return (
    <span
      className={styles.tag}
      style={{ backgroundColor: color }}
      title={`${token.raw} (${token.name})`}
    >
      {token.name}
    </span>
  );
}