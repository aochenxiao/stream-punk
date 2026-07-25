import styles from "./ErrorBanner.module.css";

interface Props {
  message: string;
  offset?: number;
  onDismiss: () => void;
}

export default function ErrorBanner({ message, offset, onDismiss }: Props) {
  return (
    <div className={styles.banner}>
      <div className={styles.content}>
        <span className={styles.icon}>&#x26A0;</span>
        <div>
          <div className={styles.title}>Parse Error</div>
          <div className={styles.message}>
            {message}
            {offset !== undefined && (
              <span className={styles.offset}> at offset {offset}</span>
            )}
          </div>
        </div>
      </div>
      <button className={styles.close} onClick={onDismiss}>
        &times;
      </button>
    </div>
  );
}