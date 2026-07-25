import { useState, useCallback } from "react";
import { parseMetaFile, ParseError } from "./parser";
import type { MetaFile } from "./parser";
import FilePicker from "./components/FilePicker";
import HeaderPanel from "./components/HeaderPanel";
import TypeList from "./components/TypeList";
import ErrorBanner from "./components/ErrorBanner";
import styles from "./App.module.css";

export default function App() {
  const [metaFile, setMetaFile] = useState<MetaFile | null>(null);
  const [error, setError] = useState<ParseError | null>(null);
  const [fileName, setFileName] = useState("");
  const [loading, setLoading] = useState(false);

  const handleFileSelect = useCallback((file: File) => {
    setFileName(file.name);
    setError(null);
    setLoading(true);

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const buffer = e.target?.result as ArrayBuffer;
        const parsed = parseMetaFile(buffer);
        setMetaFile(parsed);
        setError(null);
      } catch (err) {
        setMetaFile(null);
        if (err instanceof ParseError) {
          setError(err);
        } else if (err instanceof Error) {
          setError(new ParseError(err.message));
        } else {
          setError(new ParseError(String(err)));
        }
      } finally {
        setLoading(false);
      }
    };
    reader.onerror = () => {
      setMetaFile(null);
      setError(new ParseError("Failed to read file"));
      setLoading(false);
    };
    reader.readAsArrayBuffer(file);
  }, []);

  const dismissError = useCallback(() => setError(null), []);

  return (
    <div className={styles.app}>
      <header className={styles.header}>
        <h1 className={styles.title}>StreamPunk Meta Viewer</h1>
        <span className={styles.subtitle}>Binary Metadata Inspector</span>
      </header>

      <FilePicker onFileSelect={handleFileSelect} />

      {loading && <div className={styles.loading}>Parsing...</div>}

      {error && (
        <ErrorBanner
          message={error.message}
          offset={error.offset}
          onDismiss={dismissError}
        />
      )}

      {metaFile && (
        <>
          <HeaderPanel header={metaFile.header} fileName={fileName} />
          <TypeList types={metaFile.types} />
        </>
      )}

      {!metaFile && !error && !loading && (
        <div className={styles.placeholder}>
          <div className={styles.placeholderIcon}>&#x1F50D;</div>
          <div className={styles.placeholderText}>
            Select a StreamPunk metadata binary file to inspect its type structure
          </div>
        </div>
      )}
    </div>
  );
}