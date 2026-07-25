import type { ParsedTypeDesc } from "../parser";
import TokenTag from "./TokenTag";
import styles from "./TokenDesc.module.css";

interface Props {
  desc: ParsedTypeDesc;
}

export default function TokenDesc({ desc }: Props) {
  return <span className={styles.desc}>{renderDesc(desc)}</span>;
}

function renderDesc(desc: ParsedTypeDesc): React.ReactNode {
  if (desc.tokens.length === 0) return null;

  const first = desc.tokens[0];
  const t = first.raw;

  // Single terminal token
  if (desc.tokens.length === 1 && desc.children.length === 0) {
    return <TokenTag token={first} />;
  }

  // dur: [dur, Rep_desc..., num, den]
  if (t === 44) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        {desc.children.map((c, i) => (
          <span key={i}>{renderDesc(c)}</span>
        ))}
        <span className={styles.punct}>, </span>
        {desc.tokens.length >= 3 && (
          <>
            <TokenTag token={desc.tokens[1]} />
            <span className={styles.punct}>/</span>
            <TokenTag token={desc.tokens[2]} />
          </>
        )}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // timepoint: [timepoint, Duration_desc...]
  if (t === 45) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        {desc.children.map((c, i) => (
          <span key={i}>{renderDesc(c)}</span>
        ))}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // 1-parameter container
  if (desc.children.length === 1) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        {renderDesc(desc.children[0])}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // 2-parameter: map, umap
  if (desc.children.length === 2 && (t === 16 || t === 17)) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        {renderDesc(desc.children[0])}
        <span className={styles.punct}>, </span>
        {renderDesc(desc.children[1])}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // array: [array, N, T_desc...]
  if (t === 8 && desc.tokens.length >= 2 && desc.children.length === 1) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        <TokenTag token={desc.tokens[1]} />
        <span className={styles.punct}>, </span>
        {renderDesc(desc.children[0])}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // variant / tuple: multiple children
  if ((t === 24 || t === 25) && desc.children.length > 0) {
    return (
      <span className={styles.desc}>
        <TokenTag token={first} />
        <span className={styles.bracket}>&lt;</span>
        {desc.children.map((c, i) => (
          <span key={i}>
            {i > 0 && <span className={styles.punct}>, </span>}
            {renderDesc(c)}
          </span>
        ))}
        <span className={styles.bracket}>&gt;</span>
      </span>
    );
  }

  // Fallback: render all tokens inline
  return (
    <span className={styles.desc}>
      {desc.tokens.map((tok, i) => (
        <TokenTag key={i} token={tok} />
      ))}
    </span>
  );
}