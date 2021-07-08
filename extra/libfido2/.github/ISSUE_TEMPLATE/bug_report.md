---
name: Bug report
labels: 'bug report'
about: Report a bug in libfido2

---

<!--

Please use the questions below as a template, and review your answers
for potentially sensitive information.

Thank you!

-->

**What version of libfido2 are you using?**

**What operating system are you running?**

**What application are you using in conjunction with libfido2?**

**How does the problem manifest itself?**

**Is the problem reproducible?**

**What are the steps that lead to the problem?**

**Does the problem happen with different authenticators?**

<!--

fido2-token is provided by the fido2-tools package on Debian and Ubuntu,
and shipped with libfido2 in macOS (Homebrew), Arch Linux, and Windows.

-->

**Please include the output of `fido2-token -L`.**

<details>
<summary><code>fido2-token -L</code></summary>
<br>
<pre>
$ fido2-token -L

</pre>
</details>

**Please include the output of `fido2-token -I`.**

<details>
<summary><code>fido2-token -I</code></summary>
<br>
<pre>
$ fido2-token -I &lt;device&gt;

</pre>
</details>

<!--

You are strongly encouraged to only capture debug output using dummy
credentials. Failure to do so can disclose information such as 'I am
trying to enroll a credential for bob@silo19.nukes.military.gov' or 'I
am trying to authenticate as alice@secretserver.megaconglomerate.com,
here's a hashed challenge and signature'.

-->

**Please include the output of `FIDO_DEBUG=1`.**

<details>
<summary><code>FIDO_DEBUG=1</code></summary>
<br>
<pre>
$ export FIDO_DEBUG=1
$ &lt;command1&gt;
$ &lt;command2&gt;
(...)
$ &lt;commandn&gt;

</pre>
</details>
