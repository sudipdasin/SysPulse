# Qt system-tool integrations — validation candidate

This candidate carries the previously staged 26 optional installed system-tool entries into the now-complete mature Qt baseline. It changes the existing window and shell regression source only; original stylesheet, brand, navigation, performance and management components remain unchanged. No separate executable or UI framework. Added tabs use existing Qt workspace patterns and fixed trusted system command resolution. System utility changes remain under their own authorization and explicit review. No raw private attachment copied.

This is a source-only validation branch, not a merge or release. Native Qt build and all inherited plus added regression tests, actual XFCE tool launch behavior, administrator prompts and installation are still pending. No claim of testing successful from source copy. Do not merge until those gates pass. No GitHub Actions or CircleCI. See QT_MIGRATION_STATUS.md.
